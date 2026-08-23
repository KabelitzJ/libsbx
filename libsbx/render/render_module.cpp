// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/render_module.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <vector>

#include <libsbx/utility/logger.hpp>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/angle.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/delegate.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/window.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>

#include <libsbx/graphics/profiler.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/components.hpp>

#include <libsbx/render/depth_pre_pass.hpp>
#include <libsbx/render/light_culling_pass.hpp>
#include <libsbx/render/opaque_pass.hpp>
#include <libsbx/render/present_pass.hpp>
#include <libsbx/render/skybox_pass.hpp>
#include <libsbx/render/grid_pass.hpp>
#include <libsbx/render/tonemap_pass.hpp>
#include <libsbx/render/transparent_pass.hpp>

namespace sbx::render {

struct frame_data {
  math::matrix4x4 view;
  math::matrix4x4 projection;
  math::vector4 camera_position;
  graphics::buffer::address_type light_address;
  std::uint32_t light_count;
  std::uint32_t padding;
  graphics::buffer::address_type material_address;
  std::uint32_t irradiance_index;
  std::uint32_t brdf_lut_index;
  std::uint32_t prefiltered_index;
  std::uint32_t prefiltered_mip_count;
  std::float_t environment_intensity;
  std::uint32_t pad0;
  // Clustered Forward+ — mirrors shaders/pbr/frame_data.slang's tail exactly (field order and
  // types both matter here: there's no shared build step checking the two stay in sync).
  std::uint32_t directional_light_count;
  std::float_t cluster_scale;
  graphics::buffer::address_type cluster_range_address;
  graphics::buffer::address_type cluster_light_index_address;
  std::float_t cluster_bias;
  math::vector2 cluster_tile_size;
}; // struct frame_data

// Mirrors shaders/pbr/cluster_data.slang's cluster_aabb — compute-to-compute only, never read by
// the fragment shader, so it isn't part of frame_data.
struct cluster_aabb {
  math::vector4 min_view;
  math::vector4 max_view;
}; // struct cluster_aabb

// Mirrors shaders/pbr/cluster_data.slang's cluster_range.
struct cluster_range {
  std::uint32_t offset;
  std::uint32_t count;
}; // struct cluster_range

struct draw_bucket {
  assets::mesh_handle mesh{};
  std::uint32_t submesh_index{0u};
  assets::material_handle material{};
  std::uint32_t pipeline_id{0u};
  std::vector<math::matrix4x4> transforms{};
}; // struct draw_bucket

struct transparent_entry {
  assets::mesh_handle mesh{};
  std::uint32_t submesh_index{0u};
  assets::material_handle material{};
  std::uint32_t pipeline_id{0u};
  math::matrix4x4 transform{math::matrix4x4::identity};
  std::float_t depth{0.0f};
}; // struct transparent_entry

render_module::render_module() {
  _ensure_resources();

  // Creation order = execution order in the render thread.
  _passes.push_back(std::make_unique<depth_pre_pass>());
  // Clustered Forward+: cluster assignment only needs the camera and the light list, both already
  // known by now — runs before opaque_pass so its light lists are ready when shading needs them.
  _passes.push_back(std::make_unique<light_culling_pass>());
  _passes.push_back(std::make_unique<opaque_pass>());
  _passes.push_back(std::make_unique<skybox_pass>()); // after opaque, before transparent — see skybox_pass doc comment
  _passes.push_back(std::make_unique<grid_pass>()); // same ordering reasons as skybox_pass — see grid_pass doc comment
  _passes.push_back(std::make_unique<transparent_pass>());
  _passes.push_back(std::make_unique<tonemap_pass>());

  _composite_pass = std::make_unique<present_pass>();

  _render_thread = std::make_unique<render::render_thread>(
    core::engine::config().threading,
    [this]() { _consume_packet(_work_packet); }
  );

  _render_thread->run();
}

render_module::~render_module() {
  _render_thread->terminate();
}

auto render_module::render() -> void {
  SBX_PROFILE_SCOPE("render_module::render");

  // The iconified check is a glfw call and must stay on the main thread. When iconified we publish
  // nothing and the render thread idles.
  auto& platform_module = core::engine::get_module<platform::platform_module>();

  auto& window = platform_module.window();

  if (window.is_iconified()) {
    return;
  }

  // Throttle: wait for the previously kicked frame to fully finish before touching _work_packet
  // or anything ECS-derived again.
  _render_thread->block_until_render_complete();
  _render_thread->next_frame();

  if (_pre_render_callback) {
    // e.g. editor_module's ImGui NewFrame/build/Render — must run before _build_packet() below,
    // on this same (main) thread, so the composite pass sees this frame's finished draw data.
    std::invoke(_pre_render_callback);
  }

  _work_packet = _build_packet();

  _render_thread->kick();
}

auto render_module::set_composite_pass(std::unique_ptr<render_pass> pass) -> void {
  _composite_pass = std::move(pass);
}

auto render_module::set_pre_render_callback(core::delegate<void()> callback) -> void {
  _pre_render_callback = std::move(callback);
}

auto render_module::set_viewport_extent(math::vector2u extent) -> void {
  _viewport_extent = extent;
}

auto render_module::set_grid_enabled(bool enabled) -> void {
  _grid_enabled = enabled;
}

auto render_module::_build_packet() -> render_packet {
  SBX_PROFILE_SCOPE("render_module::build_packet");

  auto packet = render_packet{};

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  if (scene.has_active_camera()) {
    auto camera_node = scene.active_camera();

    const auto& camera = camera_node.get_component<scenes::camera>();
    const auto& world = camera_node.world_matrix();

    packet.camera.view = math::matrix4x4::inverted(world);
    packet.camera.position = math::vector3f{world[3]};
    packet.camera.fov_degrees = camera.fov_degrees;
    packet.camera.near_plane = camera.near_plane;
    packet.camera.far_plane = camera.far_plane;
    packet.camera.is_active = true;

    if (camera_node.has_component<scenes::skybox>()) {
      const auto& sky = camera_node.get_component<scenes::skybox>();

      packet.environment = sky.environment;
      packet.environment_intensity = sky.intensity;
    }
  }

  auto opaque = std::map<mesh_key, draw_bucket>{};
  auto transparent = std::vector<transparent_entry>{};

  for (const auto [entity, world, renderer] : scene.query<scenes::world_transform, scenes::mesh_renderer>().each()) {
    if (!renderer.mesh.is_valid()) {
      continue;
    }

    const auto& submeshes = renderer.mesh->submeshes();

    for (auto index = std::uint32_t{0u}; index < submeshes.size(); ++index) {
      if (index >= renderer.materials.size()) {
        continue;
      }

      const auto& material = renderer.materials[index];

      if (!material.is_valid()) {
        continue;
      }

      const auto pipeline_id = material->is_double_sided() ? 1u : 0u;

      if (material->alpha() == assets::alpha_mode::blend) {
        const auto to_camera = packet.camera.position - math::vector3f{world.matrix[3]};
        const auto depth = to_camera.length_squared();

        if (material->is_double_sided()) {
          // Two draws, same depth key: back faces (pipeline 1, cull_mode::front) must land
          // before front faces (pipeline 0, the same pipeline single-sided objects use) so the
          // shell composites correctly against itself. stable_sort below preserves this push
          // order once the cross-object depth sort runs.
          transparent.push_back(transparent_entry{renderer.mesh, index, material, 1u, world.matrix, depth});
          transparent.push_back(transparent_entry{renderer.mesh, index, material, 0u, world.matrix, depth});
        } else {
          transparent.push_back(transparent_entry{renderer.mesh, index, material, 0u, world.matrix, depth});
        }
      } else {
        auto& bucket = opaque[mesh_key{renderer.mesh->id(), index, material->id()}];
        bucket.mesh = renderer.mesh;
        bucket.submesh_index = index;
        bucket.material = material;
        bucket.pipeline_id = pipeline_id;
        bucket.transforms.push_back(world.matrix);
      }
    }
  }

  packet.opaque_commands.reserve(opaque.size());

  for (auto& [key, bucket] : opaque) {
    auto command = draw_command{};
    command.mesh = bucket.mesh;
    command.submesh_index = bucket.submesh_index;
    command.material = bucket.material;
    command.instance_count = static_cast<std::uint32_t>(bucket.transforms.size());
    command.transform_offset = static_cast<std::uint32_t>(packet.transforms.size());
    command.pipeline_id = bucket.pipeline_id;

    packet.transforms.insert(packet.transforms.end(), bucket.transforms.begin(), bucket.transforms.end());
    packet.opaque_commands.push_back(std::move(command));
  }

  // stable_sort: the double-sided pair pushed above shares an identical depth key, and
  // std::ranges::sort isn't guaranteed stable — an unstable sort could swap them and break the
  // back-faces-before-front-faces guarantee for that object.
  std::ranges::stable_sort(transparent, std::greater<>{}, &transparent_entry::depth);

  packet.transparent_commands.reserve(transparent.size());

  for (const auto& entry : transparent) {
    auto command = draw_command{};
    command.mesh = entry.mesh;
    command.submesh_index = entry.submesh_index;
    command.material = entry.material;
    command.instance_count = 1u;
    command.transform_offset = static_cast<std::uint32_t>(packet.transforms.size());
    command.pipeline_id = entry.pipeline_id;

    packet.transforms.push_back(entry.transform);
    packet.transparent_commands.push_back(std::move(command));
  }

  for (auto&& [entity, transform, light] : scene.query<scenes::world_transform, scenes::directional_light>().each()) {
    const auto& matrix = transform.matrix;
    auto& out = packet.lights.emplace_back();
  
    out.type = light_type::directional;
    out.color = math::vector4{light.color.r(), light.color.g(), light.color.b(), light.intensity};
    out.direction = math::vector4{math::vector3f::normalized(math::vector3f{-matrix[2].x(), -matrix[2].y(), -matrix[2].z()}), 0.0f};
  }

  // Clustered Forward+ relies on this exact packing order: directional lights first (always
  // evaluated in full below), then the point/spot tail cull_lights.slang actually clusters.
  packet.directional_light_count = static_cast<std::uint32_t>(packet.lights.size());

  for (auto&& [entity, transform, light] : scene.query<scenes::world_transform, scenes::point_light>().each()) {
    const auto& matrix = transform.matrix;
    auto& out = packet.lights.emplace_back();
  
    out.type = light_type::point;
    out.color = math::vector4{light.color.r(), light.color.g(), light.color.b(), light.intensity};
    out.position = math::vector4{matrix[3].x(), matrix[3].y(), matrix[3].z(), light.range};
  }

  for (auto&& [entity, transform, light] : scene.query<scenes::world_transform, scenes::spot_light>().each()) {
    const auto& matrix = transform.matrix;
    auto& out = packet.lights.emplace_back();
  
    out.type = light_type::spot;
    out.color = math::vector4{light.color.r(), light.color.g(), light.color.b(), light.intensity};
    out.position = math::vector4{matrix[3].x(), matrix[3].y(), matrix[3].z(), light.range};
    out.direction = math::vector4{math::vector3f::normalized(math::vector3f{-matrix[2].x(), -matrix[2].y(), -matrix[2].z()}), 0.0f};
    out.inner_cos = std::cos(light.inner_angle);
    out.outer_cos = std::cos(light.outer_angle);
  }

  return packet;
}

auto render_module::_consume_packet(const render_packet& packet) -> void {
  SBX_PROFILE_SCOPE("render_module::consume");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& frame_context = graphics_module.frame_context();
  auto& upload_context = graphics_module.upload_context();

  auto command_buffer = frame_context.begin_frame();

  if (!command_buffer) {
    return;
  }

  {
    SBX_PROFILE_GPU_SCOPE((*command_buffer), "render_module::render");

    const auto& swapchain = frame_context.swapchain();
    const auto swapchain_extent = swapchain.extent();

    // Falls back to the swapchain extent unless overridden (editor: the Viewport panel's size).
    const auto scene_extent = (_viewport_extent.x() > 0u && _viewport_extent.y() > 0u) ? _viewport_extent : swapchain_extent;

    assets_module.process_uploads(frame_context.frame_index());
    upload_context.flush(*command_buffer, frame_context.frame_index());

    // Baked once, synchronously, by assets_module::_bake_environment at load time (via compute
    // dispatch on the async compute queue) — the render thread just reads the resulting indices
    // straight off the environment_map asset. No more lazy first-frame bake here.
    auto irradiance_index = 0xFFFFFFFFu;
    auto brdf_lut_index = 0xFFFFFFFFu;
    auto prefiltered_index = 0xFFFFFFFFu;
    auto prefiltered_mip_count = 0u;

    if (packet.camera.is_active && packet.environment.is_valid() && assets_module.is_resident(packet.environment)) {
      irradiance_index = packet.environment->irradiance_index();
      brdf_lut_index = assets_module.brdf_lut_index();
      prefiltered_index = packet.environment->prefiltered_index();
      prefiltered_mip_count = packet.environment->prefiltered_mip_count();
    }

    _resize_targets(scene_extent);

    // Frame wrapper owns the swapchain transitions: acquire -> color attachment.
    auto to_color = graphics::command_buffer::image_transition_data{};
    to_color.image = swapchain.active_image();
    to_color.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_color.src_access_mask = VK_ACCESS_2_NONE;
    to_color.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_color.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    to_color.old_layout = graphics::image_layout::undefined;
    to_color.new_layout = graphics::image_layout::color_attachment_optimal;
    command_buffer->transition_image_layout(to_color);

    if (packet.camera.is_active) {
      const auto slot = utility::fast_mod(frame_context.frame_index(), graphics::swapchain::max_frames_in_flight);

      const auto environment_index = (packet.environment.is_valid() && assets_module.is_resident(packet.environment)) ? packet.environment->radiance_index() : 0xFFFFFFFFu;

      auto context = render_context{
        .command_buffer = command_buffer,
        .packet = memory::make_observer<const render_packet>(packet),
        .frame_index = frame_context.frame_index(),
        .slot = static_cast<std::uint32_t>(slot),
        .extent = scene_extent,
        .swapchain_extent = swapchain_extent,
        .environment_index = environment_index,
        .environment_intensity = packet.environment_intensity,
        .irradiance_index = irradiance_index,
        .brdf_lut_index = brdf_lut_index,
        .prefiltered_index = prefiltered_index,
        .prefiltered_mip_count = prefiltered_mip_count,
        .depth = _depth_image,
        .color = _color_image,
        .color_msaa = _color_msaa_image,
        .color_index = _color_index,
        .scene = _scene_image,
        .scene_index = _scene_index
      };

      _prepare_frame(context);

      for (auto& pass : _passes) {
        pass->execute(context);
      }

      if (_composite_pass) {
        _composite_pass->execute(context);
      }
    } else {
      auto color_attachment = VkRenderingAttachmentInfo{};
      color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
      color_attachment.imageView = swapchain.active_image_view();
      color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      color_attachment.clearValue.color = VkClearColorValue{{packet.clear_color.r(), packet.clear_color.g(), packet.clear_color.b(), packet.clear_color.a()}};

      auto rendering_info = VkRenderingInfo{};
      rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
      rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{swapchain_extent.x(), swapchain_extent.y()}};
      rendering_info.layerCount = 1u;
      rendering_info.colorAttachmentCount = 1u;
      rendering_info.pColorAttachments = &color_attachment;

      command_buffer->begin_rendering(rendering_info);
      command_buffer->end_rendering();
    }

    auto to_present = graphics::command_buffer::image_transition_data{};
    to_present.image = swapchain.active_image();
    to_present.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_present.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    to_present.dst_stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    to_present.dst_access_mask = VK_ACCESS_2_NONE;
    to_present.old_layout = graphics::image_layout::color_attachment_optimal;
    to_present.new_layout = graphics::image_layout::present_source;
    command_buffer->transition_image_layout(to_present);
  }

  frame_context.end_frame();
}

auto render_module::_ensure_resources() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();
  auto& registry = graphics_module.resource_registry();

  _sampler_index = bindless_table.sampler_index(graphics::sampler::create_info{
    .max_anisotropy = 16.0f,
    .name = "Material Sampler"
  });

  _clamp_sampler_index = bindless_table.sampler_index(graphics::sampler::create_info{
    .address_mode_u = graphics::address_mode::clamp_to_edge,
    .address_mode_v = graphics::address_mode::clamp_to_edge,
    .address_mode_w = graphics::address_mode::clamp_to_edge,
    .max_lod = 1.0f,
    .name = "Clamp Sampler"
  });

  _color_index = bindless_table.reserve_sampled_image();

  _scene_index = bindless_table.reserve_sampled_image();

  _frame_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<frame_data> * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Frame Data"
  });

  const auto frame_base = registry.get<graphics::buffer>(_frame_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _frame_addresses[slot] = frame_base + slot * memory::stride_v<frame_data>;
  }

  _light_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<light_data> * light_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Light Data"
  });

  const auto light_base = registry.get<graphics::buffer>(_light_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _light_addresses[slot] = light_base + slot * light_capacity * memory::stride_v<light_data>;
  }

  _transform_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(math::matrix4x4) * transform_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Instance Transforms"
  });

  const auto transform_base = registry.get<graphics::buffer>(_transform_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _transform_addresses[slot] = transform_base + slot * transform_capacity * sizeof(math::matrix4x4);
  }

  // Clustered Forward+ buffers. All three are written only by light_culling_pass's compute
  // dispatches and read only by that same frame's later passes, never by the CPU — device_local,
  // not host_write, unlike the buffers above.
  _cluster_aabb_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<cluster_aabb> * cluster_count * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Cluster AABBs"
  });

  const auto cluster_aabb_base = registry.get<graphics::buffer>(_cluster_aabb_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _cluster_aabb_addresses[slot] = cluster_aabb_base + slot * cluster_count * memory::stride_v<cluster_aabb>;
  }

  _cluster_range_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<cluster_range> * cluster_count * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Cluster Ranges"
  });

  const auto cluster_range_base = registry.get<graphics::buffer>(_cluster_range_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _cluster_range_addresses[slot] = cluster_range_base + slot * cluster_count * memory::stride_v<cluster_range>;
  }

  _cluster_light_index_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<std::uint32_t> * cluster_light_index_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = "Cluster Light Indices"
  });

  const auto cluster_light_index_base = registry.get<graphics::buffer>(_cluster_light_index_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _cluster_light_index_addresses[slot] = cluster_light_index_base + slot * cluster_light_index_capacity * memory::stride_v<std::uint32_t>;
  }

  // The one cluster buffer the CPU does touch: reset to 0 every frame in _prepare_frame, exactly
  // like the buffers above, since cull_lights.slang's atomic reserve has to start counting there.
  _cluster_counter_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<std::uint32_t> * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Cluster Counter"
  });

  const auto cluster_counter_base = registry.get<graphics::buffer>(_cluster_counter_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _cluster_counter_addresses[slot] = cluster_counter_base + slot * memory::stride_v<std::uint32_t>;
  }
}

auto render_module::_resize_targets(const math::vector2u extent) -> void {
  if (_target_extent == extent) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& surface = graphics_module.surface();
  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& frame_context = graphics_module.frame_context();
  const auto frame_index = frame_context.frame_index();

  if (_target_extent != math::vector2u{0u, 0u}) {
    registry.retire(_depth_image, frame_index);
    registry.retire(_color_image, frame_index);
    registry.retire(_color_msaa_image, frame_index);
    registry.retire(_scene_image, frame_index);
  }

  _depth_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::d32_sfloat,
    .usage = graphics::image_usage::depth_stencil_attachment,
    .samples = render_pass::sample_count,
    .name = "Depth"
  });

  _color_msaa_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::color_attachment,
    .samples = render_pass::sample_count,
    .name = "HDR Color MSAA"
  });

  _color_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .samples = graphics::samples::count_1,
    .name = "HDR Color Resolve"
  });

  bindless_table.write_sampled_image(_color_index, registry.get<graphics::image>(_color_image).view());

  const auto scene_format = static_cast<graphics::format>(surface.format().format);

  _scene_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = scene_format,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .samples = graphics::samples::count_1,
    .name = "Scene Color"
  });

  bindless_table.write_sampled_image(_scene_index, registry.get<graphics::image>(_scene_image).view());

  _target_extent = extent;
}

auto render_module::_prepare_frame(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& registry = graphics_module.resource_registry();

  const auto aspect = static_cast<std::float_t>(context.extent.x()) / static_cast<std::float_t>(context.extent.y());
  const auto projection = math::matrix4x4::perspective(math::degree{context.packet->camera.fov_degrees}, aspect, context.packet->camera.near_plane, context.packet->camera.far_plane);

  const auto light_count = std::min(static_cast<std::uint32_t>(context.packet->lights.size()), light_capacity);

  if (light_count > 0u) {
    auto& light_buffer = registry.get<graphics::buffer>(_light_buffer);

    light_buffer.write(context.packet->lights.data(), light_count * memory::stride_v<light_data>, context.slot * light_capacity * memory::stride_v<light_data>);
  }

  const auto instance_count = std::min(static_cast<std::uint32_t>(context.packet->transforms.size()), transform_capacity);

  if (instance_count > 0u) {
    auto& transform_buffer = registry.get<graphics::buffer>(_transform_buffer);

    transform_buffer.write(context.packet->transforms.data(), instance_count * sizeof(math::matrix4x4), context.slot * transform_capacity * sizeof(math::matrix4x4));
  }

  // Clustered Forward+: directional lights always precede the point/spot tail (see
  // _build_packet), clamped the same way light_count is in case the total was itself truncated.
  const auto directional_light_count = std::min(context.packet->directional_light_count, light_count);

  // Exponential depth-slice formula (see build_clusters.slang): folded into two scalars here so
  // every fragment doesn't have to redo log2(far/near) and a division of its own.
  const auto& camera = context.packet->camera;
  const auto slice_ratio = std::log2(camera.far_plane / camera.near_plane);
  const auto cluster_scale = static_cast<std::float_t>(cluster_dim_z) / slice_ratio;
  const auto cluster_bias = -static_cast<std::float_t>(cluster_dim_z) * std::log2(camera.near_plane) / slice_ratio;

  const auto cluster_tile_size = math::vector2{
    static_cast<std::float_t>(context.extent.x()) / static_cast<std::float_t>(cluster_dim_x),
    static_cast<std::float_t>(context.extent.y()) / static_cast<std::float_t>(cluster_dim_y)
  };

  // Reset before this frame's light_culling_pass dispatches its atomic-reserving cull_lights.slang.
  const auto counter_reset = std::uint32_t{0u};
  auto& cluster_counter_buffer = registry.get<graphics::buffer>(_cluster_counter_buffer);
  cluster_counter_buffer.write(&counter_reset, sizeof(counter_reset), context.slot * memory::stride_v<std::uint32_t>);

  auto data = frame_data{};
  data.view = context.packet->camera.view;
  data.projection = projection;
  data.camera_position = math::vector4{context.packet->camera.position, 1.0f};
  data.light_address = _light_addresses[context.slot];
  data.light_count = light_count;
  data.material_address = assets_module.material_buffer_address();
  data.irradiance_index = context.irradiance_index;
  data.brdf_lut_index = context.brdf_lut_index;
  data.prefiltered_index = context.prefiltered_index;
  data.prefiltered_mip_count = context.prefiltered_mip_count;
  data.environment_intensity = context.environment_intensity;
  data.pad0 = 0u;
  data.directional_light_count = directional_light_count;
  data.cluster_scale = cluster_scale;
  data.cluster_range_address = _cluster_range_addresses[context.slot];
  data.cluster_light_index_address = _cluster_light_index_addresses[context.slot];
  data.cluster_bias = cluster_bias;
  data.cluster_tile_size = cluster_tile_size;

  auto& frame_buffer = registry.get<graphics::buffer>(_frame_buffer);
  frame_buffer.write(&data, sizeof(frame_data), context.slot * memory::stride_v<frame_data>);

  context.frame_address = _frame_addresses[context.slot];
  context.transform_address = _transform_addresses[context.slot];
  context.instance_count = instance_count;
  context.sampler_index = _sampler_index;
  context.clamp_sampler_index = _clamp_sampler_index;
  context.show_grid = _grid_enabled;
  context.inverse_view_projection = math::matrix4x4::inverted(projection * context.packet->camera.view);

  context.cluster_aabb_address = _cluster_aabb_addresses[context.slot];
  context.cluster_range_address = data.cluster_range_address;
  context.cluster_light_index_address = data.cluster_light_index_address;
  context.cluster_counter_address = _cluster_counter_addresses[context.slot];
}

} // namespace sbx::render
