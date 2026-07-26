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

namespace sbx::render {

struct frame_data {
  math::matrix4x4 view;
  math::matrix4x4 projection;
  math::vector4 camera_position;
  graphics::buffer::address_type light_address;
  std::uint32_t light_count;
  std::uint32_t padding;
  graphics::buffer::address_type material_address;
}; // struct frame_data

struct push_constants {
  graphics::buffer::address_type frame_address;
  graphics::buffer::address_type vertex_address;
  graphics::buffer::address_type transform_address;
  std::uint32_t transform_offset;
  std::uint32_t material_index;
  std::uint32_t sampler_index;
}; // struct push_constants

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
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();
  auto& registry = graphics_module.resource_registry();
  auto& surface = graphics_module.surface();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = graphics_module.shader_cache().get({"shaders/pbr/pbr.slang", entry_points});

  const auto make_pipeline = [&](bool is_transparent, graphics::cull_mode cull, const std::string& name) {
    auto info = graphics::graphics_pipeline::create_info{
      .shader = shader,
      .color_formats = {static_cast<graphics::format>(surface.format().format)},
      .depth_format = graphics::format::d32_sfloat,
      .cull_mode = cull,
      .front_face = graphics::front_face::counter_clockwise,
      .depth_test = true,
      .depth_write = !is_transparent,
      .name = name
    };

    if (is_transparent) {
      info.color_blend_attachments = {graphics::blend_attachment{
        .enable = true,
        .source_color = graphics::blend_factor::source_alpha,
        .destination_color = graphics::blend_factor::one_minus_source_alpha,
        .color_operation = graphics::blend_operation::add,
        .source_alpha = graphics::blend_factor::one,
        .destination_alpha = graphics::blend_factor::one_minus_source_alpha,
        .alpha_operation = graphics::blend_operation::add
      }};
    }

    return graphics_module.pipeline_cache().get(info);
  };

  _opaque_pipelines[0] = make_pipeline(false, graphics::cull_mode::back, "Mesh Opaque");
  _opaque_pipelines[1] = make_pipeline(false, graphics::cull_mode::none, "Mesh Opaque Double-Sided");
  _transparent_pipelines[0] = make_pipeline(true, graphics::cull_mode::back, "Mesh Transparent");
  _transparent_pipelines[1] = make_pipeline(true, graphics::cull_mode::none, "Mesh Transparent Double-Sided");

  _sampler_index = bindless_table.sampler_index(graphics::sampler::create_info{});

  _frame_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<frame_data> * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = fmt::format("Frame Data")
  });

  const auto base = registry.get<graphics::buffer>(_frame_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _frame_addresses[slot] = base + slot * memory::stride_v<frame_data>;
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
    .size = memory::stride_v<math::matrix4x4> * transform_capacity * graphics::swapchain::max_frames_in_flight,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Instance Transforms"
  });

  const auto transform_base = registry.get<graphics::buffer>(_transform_buffer).address();

  for (auto slot = std::size_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _transform_addresses[slot] = transform_base + slot * transform_capacity * memory::stride_v<math::matrix4x4>;
  }
}

render_module::~render_module() {
  _stop();
}

auto render_module::render() -> void {
  SBX_PROFILE_SCOPE("render_module::render");

  if (!_is_started) {
    _start();
  }

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
  _is_started = true;

  _thread = std::thread{[this]() { _render_loop(); }};
}

auto render_module::_stop() -> void {
  if (!_is_started) {
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
    packet.camera.active = true;
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
  auto& bindless_table = graphics_module.bindless_table();

  auto command_buffer = frame_context.begin_frame();

  if (!command_buffer) {
    return;
  }

  {
    SBX_PROFILE_GPU_SCOPE((*command_buffer), "render_module::render");

    const auto& swapchain = frame_context.swapchain();
    const auto extent = swapchain.extent();

    // Turn queued asset uploads into GPU images/buffers + bindless writes, then record their copies.
    assets_module.process_uploads(frame_context.frame_index());

    upload_context.flush(*command_buffer, frame_context.frame_index());

    // (Re)create the depth target to match the swapchain, retiring the old one on resize.
    if (_depth_width != extent.width || _depth_height != extent.height) {
      if (_depth_width != 0u) {
        registry.retire(_depth_image, frame_context.frame_index());
      }

      _depth_image = registry.emplace<graphics::image>(graphics::image::create_info{
        .extent = math::vector3u{extent.width, extent.height, 1u},
        .format = graphics::format::d32_sfloat,
        .usage = graphics::image_usage::depth_stencil_attachment,
        .name = "Depth"
      });

      _depth_width = extent.width;
      _depth_height = extent.height;
    }

    auto& depth = registry.get<graphics::image>(_depth_image);

    auto to_depth = graphics::command_buffer::image_transition_data{};
    to_depth.image = depth.handle();
    to_depth.src_stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    to_depth.src_access_mask = VK_ACCESS_2_NONE;
    to_depth.dst_stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    to_depth.dst_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    to_depth.old_layout = graphics::image_layout::undefined;
    to_depth.new_layout = graphics::image_layout::depth_attachment_optimal;
    to_depth.aspect_mask = depth.aspect();
    to_depth.layer_count = 1u;

    command_buffer->transition_image_layout(to_depth);

    auto to_color = graphics::command_buffer::image_transition_data{};
    to_color.image = swapchain.active_image();
    to_color.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_color.src_access_mask = VK_ACCESS_2_NONE;
    to_color.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_color.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    to_color.old_layout = graphics::image_layout::undefined;
    to_color.new_layout = graphics::image_layout::color_attachment_optimal;

    command_buffer->transition_image_layout(to_color);

    auto color_attachment = VkRenderingAttachmentInfo{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = swapchain.active_image_view();
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue.color = VkClearColorValue{{packet.clear_color.r(), packet.clear_color.g(), packet.clear_color.b(), packet.clear_color.a()}};

    auto depth_attachment = VkRenderingAttachmentInfo{};
    depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attachment.imageView = depth.view();
    depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.clearValue.depthStencil = VkClearDepthStencilValue{1.0f, 0u};

    auto rendering_info = VkRenderingInfo{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, extent};
    rendering_info.layerCount = 1u;
    rendering_info.colorAttachmentCount = 1u;
    rendering_info.pColorAttachments = &color_attachment;
    rendering_info.pDepthAttachment = &depth_attachment;

    command_buffer->begin_rendering(rendering_info);

    if (packet.camera.active) {
      const auto aspect = static_cast<std::float_t>(extent.width) / static_cast<std::float_t>(extent.height);
      const auto projection = math::matrix4x4::perspective(math::degree{packet.camera.fov_degrees}, aspect, packet.camera.near_plane, packet.camera.far_plane);
      const auto& view = packet.camera.view;

      // Write this frame's data into its slot buffer. The throttle guarantees the slot from two frames ago is done, so this never races the GPU.
      const auto slot = utility::fast_mod(frame_context.frame_index(), graphics::swapchain::max_frames_in_flight);

      const auto light_count = std::min(static_cast<std::uint32_t>(packet.lights.size()), light_capacity);

      if (light_count > 0u) {
        auto& light_buffer = registry.get<graphics::buffer>(_light_buffer);

        light_buffer.write(packet.lights.data(), light_count * memory::stride_v<light_data>, slot * light_capacity * memory::stride_v<light_data>);
      }

      auto data = frame_data{};
      data.view = view;
      data.projection = projection;
      data.camera_position = math::vector4{packet.camera.position, 1.0f};
      data.light_address = _light_addresses[slot];
      data.light_count = light_count;
      data.material_address = assets_module.material_buffer_address();

      auto& frame_buffer = registry.get<graphics::buffer>(_frame_buffer);
      frame_buffer.write(&data, sizeof(frame_data), slot * memory::stride_v<frame_data>);

      const auto instance_count = std::min(static_cast<std::uint32_t>(packet.transforms.size()), transform_capacity);

      if (instance_count > 0u) {
        auto& transform_buffer = registry.get<graphics::buffer>(_transform_buffer);

        transform_buffer.write(packet.transforms.data(), instance_count * memory::stride_v<math::matrix4x4>, slot * transform_capacity * memory::stride_v<math::matrix4x4>);
      }

      if (!packet.opaque_commands.empty() || !packet.transparent_commands.empty()) {
        const auto descriptor_set = bindless_table.descriptor_set();
        vkCmdBindDescriptorSets(*command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

        const auto viewport = VkViewport{0.0f, 0.0f, static_cast<std::float_t>(extent.width), static_cast<std::float_t>(extent.height), 0.0f, 1.0f};
        command_buffer->set_viewport(viewport);

        const auto scissor = VkRect2D{VkOffset2D{0, 0}, extent};
        command_buffer->set_scissor(scissor);
      }

      const auto draw_list = [&](const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u>& pipelines) {
        auto bound = false;
        auto current_pipeline = std::uint32_t{0u};
        const auto* current_mesh = static_cast<const assets::mesh*>(nullptr);

        for (const auto& command : commands) {
          if (!command.mesh.is_valid() || !assets_module.is_resident(command.mesh)) {
            continue;
          }

          if (!command.material.is_valid() || !assets_module.is_resident(command.material)) {
            continue;
          }

          if (command.transform_offset + command.instance_count > instance_count) {
            continue;
          }

          if (!bound || current_pipeline != command.pipeline_id) {
            command_buffer->bind_pipeline(*pipelines[command.pipeline_id]);
            current_pipeline = command.pipeline_id;
            bound = true;
          }

          if (current_mesh != command.mesh.get()) {
            vkCmdBindIndexBuffer(*command_buffer, registry.get<graphics::buffer>(command.mesh->index_buffer()), 0u, VK_INDEX_TYPE_UINT32);
            current_mesh = command.mesh.get();
          }

          const auto& submesh = command.mesh->submeshes()[command.submesh_index];

          auto values = push_constants{};
          values.frame_address = _frame_addresses[slot];
          values.vertex_address = command.mesh->vertex_address();
          values.transform_address = _transform_addresses[slot];
          values.transform_offset = command.transform_offset;
          values.material_index = command.material->index();
          values.sampler_index = _sampler_index;

          auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
          std::memcpy(range.data(), &values, sizeof(push_constants));

          vkCmdPushConstants(*command_buffer, bindless_table.pipeline_layout(), VK_SHADER_STAGE_ALL, 0u, static_cast<std::uint32_t>(range.size()), range.data());

          vkCmdDrawIndexed(*command_buffer, submesh.index_count, command.instance_count, submesh.index_offset, 0, 0u);
        }
      };

      draw_list(packet.opaque_commands, _opaque_pipelines);
      draw_list(packet.transparent_commands, _transparent_pipelines);
    }

    command_buffer->end_rendering();

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

} // namespace sbx::render
