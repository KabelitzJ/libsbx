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
#include <libsbx/render/geometry_pass.hpp>
#include <libsbx/render/tonemap_pass.hpp>
#include <libsbx/render/skybox_pass.hpp>

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
  std::uint32_t prefiltered_base_index;
  std::uint32_t prefiltered_count;
  std::float_t environment_intensity;
  std::uint32_t pad0;
}; // struct frame_data

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

struct ibl_convolve_push { 
  std::uint32_t environment_index; 
  std::uint32_t sampler_index;
}; // struct ibl_convolve_push

struct ibl_prefilter_push { 
  std::uint32_t environment_index; 
  std::uint32_t sampler_index; 
  std::float_t roughness; 
}; // struct ibl_prefilter_push

render_module::render_module() {
  _ensure_resources();
  _ensure_ibl_pipelines();

  // Creation order = execution order in the render thread.
  _passes.push_back(std::make_unique<depth_pre_pass>());
  _passes.push_back(std::make_unique<geometry_pass>());
  _passes.push_back(std::make_unique<skybox_pass>());
  _passes.push_back(std::make_unique<tonemap_pass>());

  _start();
}

render_module::~render_module() {
  _stop();
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

  auto packet = _build_packet();

  {
    auto lock = std::unique_lock{_mutex};

    // Throttle: wait until the render thread has taken the previous packet.
    _has_consumed.wait(lock, [this]() { return !_has_packet; });

    _packet = std::move(packet);
    _has_packet = true;
  }

  _has_produced.notify_one();
}

auto render_module::_start() -> void {
  _is_running = true;

  _thread = std::thread{[this]() { _render_loop(); }};
}

auto render_module::_stop() -> void {
  if (!_is_running) {
    return;
  }

  {
    auto lock = std::unique_lock{_mutex};
    _is_running = false;
  }

  _has_produced.notify_one();

  if (_thread.joinable()) {
    _thread.join();
  }
}

auto render_module::_render_loop() -> void {
  SBX_PROFILE_THREAD_NAME("Render thread");

  while (true) {
    auto packet = render_packet{};

    {
      auto lock = std::unique_lock{_mutex};

      _has_produced.wait(lock, [this]() { return _has_packet || !_is_running; });

      if (!_has_packet && !_is_running) {
        break;
      }

      packet = std::move(_packet);
      _has_packet = false;
    }

    // Let the main thread produce the next packet while this frame renders.
    _has_consumed.notify_one();

    _consume_packet(packet);
  }
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
      const auto& submesh = submeshes[index];

      const auto& material = (index < renderer.materials.size() && renderer.materials[index].is_valid()) ? renderer.materials[index] : submesh.material;

      if (!material.is_valid()) {
        continue;
      }

      const auto pipeline_id = material->is_double_sided() ? 1u : 0u;

      if (material->alpha() == assets::alpha_mode::blend) {
        const auto translation = world.matrix[3];
        const auto to_camera = packet.camera.position - math::vector3f{world.matrix[3]};
        const auto depth = to_camera.length_squared();

        transparent.push_back(transparent_entry{renderer.mesh, index, material, pipeline_id, world.matrix, depth});
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

  std::ranges::sort(transparent, std::greater<>{}, &transparent_entry::depth);

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
  auto& registry = graphics_module.resource_registry();
  auto& upload_context = graphics_module.upload_context();

  auto command_buffer = frame_context.begin_frame();

  if (!command_buffer) {
    return;
  }

  {
    SBX_PROFILE_GPU_SCOPE((*command_buffer), "render_module::render");

    const auto& swapchain = frame_context.swapchain();
    const auto extent = swapchain.extent();

    assets_module.process_uploads(frame_context.frame_index());
    upload_context.flush(*command_buffer, frame_context.frame_index());

    auto irradiance_index = 0xFFFFFFFFu;
    auto brdf_lut_index = 0xFFFFFFFFu;
    auto prefiltered_base = 0u;
    auto prefiltered_count = 0u;

    if (packet.camera.is_active && packet.environment.is_valid() && assets_module.is_resident(packet.environment)) {
      _ensure_brdf_lut(*command_buffer);

      const auto id = packet.environment->id();

      if (!_baked_environments.contains(id)) {
        _bake_environment(*command_buffer, id, packet.environment->radiance_index());
      }

      const auto& baked = _baked_environments.at(id);

      irradiance_index = baked.irradiance_index;
      brdf_lut_index = _brdf_lut_index;
      prefiltered_base = baked.prefiltered_base;
      prefiltered_count = baked.prefiltered_count;
    }

    _resize_targets(extent);

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
        .extent = extent,
        .environment_index = environment_index,
        .environment_intensity = packet.environment_intensity,
        .irradiance_index = irradiance_index,
        .brdf_lut_index = brdf_lut_index,
        .prefiltered_base_index = prefiltered_base,
        .prefiltered_count = prefiltered_count,
        .depth = _depth_image,
        .color = _color_image,
        .color_index = _color_index
      };

      _prepare_frame(context);

      for (auto& pass : _passes) {
        pass->execute(context);
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
      rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{extent.x(), extent.y()}};
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
  auto& surface = graphics_module.surface();

  _sampler_index = bindless_table.sampler_index(graphics::sampler::create_info{});

  _color_index = bindless_table.reserve_sampled_image();

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
}

auto render_module::_resize_targets(const math::vector2u extent) -> void {
  if (_target_extent == extent) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& frame_context = graphics_module.frame_context();
  const auto frame_index = frame_context.frame_index();

  if (_target_extent != math::vector2u{0u, 0u}) {
    registry.retire(_depth_image, frame_index);
    registry.retire(_color_image, frame_index);
  }

  _depth_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::d32_sfloat,
    .usage = graphics::image_usage::depth_stencil_attachment,
    .name = "Depth"
  });

  _color_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .name = "HDR Color"
  });

  bindless_table.write_sampled_image(_color_index, registry.get<graphics::image>(_color_image).view());

  _target_extent = extent;
}

auto render_module::_prepare_frame(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& registry = graphics_module.resource_registry();

  const auto aspect = static_cast<float>(context.extent.x()) / static_cast<float>(context.extent.y());
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

  auto data = frame_data{};
  data.view = context.packet->camera.view;
  data.projection = projection;
  data.camera_position = math::vector4{context.packet->camera.position, 1.0f};
  data.light_address = _light_addresses[context.slot];
  data.light_count = light_count;
  data.material_address = assets_module.material_buffer_address();
  data.irradiance_index = context.irradiance_index;
  data.brdf_lut_index = context.brdf_lut_index;
  data.prefiltered_base_index = context.prefiltered_base_index;
  data.prefiltered_count = context.prefiltered_count;
  data.environment_intensity = context.environment_intensity;
  data.pad0 = 0u;

  auto& frame_buffer = registry.get<graphics::buffer>(_frame_buffer);
  frame_buffer.write(&data, sizeof(frame_data), context.slot * memory::stride_v<frame_data>);

  context.frame_address = _frame_addresses[context.slot];
  context.transform_address = _transform_addresses[context.slot];
  context.instance_count = instance_count;
  context.sampler_index = _sampler_index;
  context.inverse_view_projection = math::matrix4x4::inverted(projection * context.packet->camera.view);
}

auto render_module::_ensure_ibl_pipelines() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto make = [&](const std::filesystem::path& path, graphics::format format, const std::string& name) {
    const auto& shader = shader_cache.get({path, entry_points});
    return pipeline_cache.get(graphics::graphics_pipeline::create_info{
      .shader = shader,
      .color_formats = {format},
      .cull_mode = graphics::cull_mode::none,
      .depth_test = false,
      .depth_write = false,
      .name = name
    });
  };

  _brdf_lut_pipeline = make("shaders/pbr/brdf_lut.slang", graphics::format::r16g16_sfloat, "IBL BRDF LUT");
  _irradiance_pipeline = make("shaders/pbr/irradiance.slang", graphics::format::r16g16b16a16_sfloat, "IBL Irradiance");
  _prefilter_pipeline = make("shaders/pbr/prefilter.slang", graphics::format::r16g16b16a16_sfloat, "IBL Prefilter");
}

auto render_module::_bake_fullscreen(graphics::command_buffer& command_buffer, graphics::image& target, const math::vector2u& extent, graphics::graphics_pipeline& pipeline, std::span<const std::byte> push_data) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    
  auto& bindless_table = graphics_module.bindless_table();

  auto to_color = graphics::command_buffer::image_transition_data{};
  to_color.image = target.handle();
  to_color.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color.src_access_mask = VK_ACCESS_2_NONE;
  to_color.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_color.old_layout = graphics::image_layout::undefined;
  to_color.new_layout = graphics::image_layout::color_attachment_optimal;
  to_color.aspect_mask = target.aspect();
  to_color.layer_count = 1u;
  command_buffer.transition_image_layout(to_color);

  auto attachment = VkRenderingAttachmentInfo{};
  attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  attachment.imageView = target.view();
  attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{extent.x(), extent.y()}};
  rendering_info.layerCount = 1u;
  rendering_info.colorAttachmentCount = 1u;
  rendering_info.pColorAttachments = &attachment;

  command_buffer.begin_rendering(rendering_info);

  const auto descriptor_set = bindless_table.descriptor_set();

  vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  command_buffer.set_viewport(VkViewport{0.0f, 0.0f, static_cast<std::float_t>(extent.x()), static_cast<std::float_t>(extent.y()), 0.0f, 1.0f});
  command_buffer.set_scissor(VkRect2D{VkOffset2D{0, 0}, VkExtent2D{extent.x(), extent.y()}});
  command_buffer.bind_pipeline(pipeline);

  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), push_data.data(), push_data.size());
  
  command_buffer.push_constants(bindless_table.pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0u, range);

  command_buffer.draw(3u, 1u, 0u, 0u);
  
  command_buffer.end_rendering();

  auto to_read = graphics::command_buffer::image_transition_data{};
  to_read.image = target.handle();
  to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_read.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  to_read.dst_access_mask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  to_read.old_layout = graphics::image_layout::color_attachment_optimal;
  to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
  to_read.aspect_mask = target.aspect();
  to_read.layer_count = 1u;
  command_buffer.transition_image_layout(to_read);
}

auto render_module::_ensure_brdf_lut(graphics::command_buffer& command_buffer) -> void {
  if (_brdf_lut_index != 0xFFFFFFFFu) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();

  const auto extent = math::vector2u{512u, 512u};

  _brdf_lut_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{extent.x(), extent.y(), 1u},
    .format = graphics::format::r16g16_sfloat,
    .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
    .name = "IBL BRDF LUT"
  });

  _brdf_lut_index = bindless_table.reserve_sampled_image();

  _bake_fullscreen(command_buffer, registry.get<graphics::image>(_brdf_lut_image), extent, *_brdf_lut_pipeline, std::span<const std::byte>{});

  bindless_table.write_sampled_image(_brdf_lut_index, registry.get<graphics::image>(_brdf_lut_image).view());
}

auto render_module::_bake_environment(graphics::command_buffer& command_buffer, const math::uuid& id, std::uint32_t radiance_index) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();

  auto baked = baked_environment{};
  baked.prefiltered_count = prefiltered_levels;

  // Irradiance.
  {
    const auto extent = math::vector2u{64u, 64u};

    baked.irradiance_image = registry.emplace<graphics::image>(graphics::image::create_info{
      .extent = math::vector3u{extent, 1u},
      .format = graphics::format::r16g16b16a16_sfloat,
      .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
      .name = "IBL Irradiance"
    });

    baked.irradiance_index = bindless_table.reserve_sampled_image();

    auto push = ibl_convolve_push{};
    push.environment_index = radiance_index;
    push.sampler_index = _sampler_index;

    _bake_fullscreen(command_buffer, registry.get<graphics::image>(baked.irradiance_image), extent, *_irradiance_pipeline, std::span<const std::byte>{reinterpret_cast<const std::byte*>(&push), sizeof(push)});

    bindless_table.write_sampled_image(baked.irradiance_index, registry.get<graphics::image>(baked.irradiance_image).view());
  }

  // Prefiltered specular: N separate roughness textures with consecutive bindless indices.
  baked.prefiltered_images.reserve(prefiltered_levels);

  for (auto level = std::uint32_t{0u}; level < prefiltered_levels; ++level) {
    const auto extent = math::vector2u{512u, 512u};

    const auto image = registry.emplace<graphics::image>(graphics::image::create_info{
      .extent = math::vector3u{extent, 1u},
      .format = graphics::format::r16g16b16a16_sfloat,
      .usage = graphics::image_usage::color_attachment | graphics::image_usage::sampled,
      .name = "IBL Prefiltered"
    });

    const auto index = bindless_table.reserve_sampled_image();

    if (level == 0u) {
      baked.prefiltered_base = index; // subsequent reserves are consecutive
    }

    const auto roughness = static_cast<std::float_t>(level) / static_cast<std::float_t>(prefiltered_levels - 1u);

    auto push = ibl_prefilter_push{};
    push.environment_index = radiance_index;
    push.sampler_index = _sampler_index;
    push.roughness = roughness;

    _bake_fullscreen(command_buffer, registry.get<graphics::image>(image), extent, *_prefilter_pipeline, std::span<const std::byte>{reinterpret_cast<const std::byte*>(&push), sizeof(push)});

    bindless_table.write_sampled_image(index, registry.get<graphics::image>(image).view());
    baked.prefiltered_images.push_back(image);
  }

  _baked_environments.emplace(id, std::move(baked));

  utility::logger<"render">::info("Baked IBL for environment {}", id);
}

} // namespace sbx::render
