// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/render_module.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/math/vector3.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/window.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/profiler.hpp>

namespace sbx::render {

struct push_constants {
  std::uint32_t image_index;
  std::uint32_t sampler_index;
}; // struct push_constants

render_module::render_module() { }

render_module::~render_module() {
  _stop();
}

auto render_module::render() -> void {
  SBX_PROFILE_SCOPE("render_module::render");

  if (!_is_started) {
    _start();
  }

  // The iconified check is a glfw call and must stay on the main thread. 
  // When iconified we simply publish nothing and the render thread idles.
  auto& platform_module = core::engine::get_module<platform::platform_module>();

  auto& window = platform_module.window();

  if (window.is_iconified()) {
    return;
  }

  const auto packet = _build_packet();

  {
    auto lock = std::unique_lock{_mutex};

    // Throttle: wait until the render thread has taken the previous packet.
    _has_consumed.wait(lock, [this]() { return !_has_packet; });

    _packet = std::move(packet);
    _has_packet = true;
  }

  _has_produced.notify_one();
}

auto render_module::upload_texture(std::vector<std::byte> pixels, std::uint32_t width, std::uint32_t height, graphics::format format) -> void {
  auto lock = std::lock_guard{_texture_mutex};

  _pending_texture = pending_texture{std::move(pixels), width, height, format};
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

  packet.clear_color = math::color{0.7f, 0.2f, 0.2f, 1.0f};

  return packet;
}

auto render_module::_consume_packet(const render_packet& packet) -> void {
  SBX_PROFILE_SCOPE("render_module::consume");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

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

    if (_pipeline == nullptr) {
      const auto entry_points = std::array<sbx::graphics::shader_compiler::entry_point_request, 2u>{
        sbx::graphics::shader_compiler::entry_point_request{"vertex_main", VK_SHADER_STAGE_VERTEX_BIT},
        sbx::graphics::shader_compiler::entry_point_request{"fragment_main", VK_SHADER_STAGE_FRAGMENT_BIT}
      };

      _shader = std::make_unique<sbx::graphics::shader>(graphics_module.shader_compiler(), "shaders/triangle/triangle.slang", entry_points);

      _pipeline = std::make_unique<sbx::graphics::graphics_pipeline>(sbx::graphics::graphics_pipeline::create_info{
        .shader = _shader.get(),
        .color_formats = {static_cast<sbx::graphics::format>(swapchain.format())},
        .name = "Triangle"
      });
    }

    // Pick up the texture posted from the main thread, create + upload it, and register it in the
    // bindless table. Record the frame so we know when it is resident.
    if (!_has_texture) {
      auto pending = std::optional<pending_texture>{};

      {
        auto lock = std::lock_guard{_texture_mutex};
        pending.swap(_pending_texture);
      }

      if (pending) {
        _texture = registry.emplace<sbx::graphics::image>(sbx::graphics::image::create_info{
          .extent = sbx::math::vector3u{pending->width, pending->height, 1u},
          .format = pending->format,
          .usage = sbx::graphics::image_usage::transfer_destination | sbx::graphics::image_usage::sampled,
          .name = "Display Texture"
        });

        const auto bytes = std::span<const std::byte>{pending->pixels.data(), pending->pixels.size()};

        upload_context.stage_image(_texture, bytes, sbx::graphics::image_layout::shader_read_only_optimal);

        const auto& texture = registry.get<sbx::graphics::image>(_texture);

        _texture_index = bindless_table.register_sampled_image(texture.view());
        _sampler_index = bindless_table.sampler_index(sbx::graphics::sampler::create_info{});
        _texture_frame = frame_context.frame_index();
        _has_texture = true;
      }
    }

    upload_context.flush(*command_buffer, frame_context.frame_index());

    auto to_color = sbx::graphics::command_buffer::image_transition_data{};
    to_color.image = swapchain.active_image();
    to_color.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_color.src_access_mask = VK_ACCESS_2_NONE;
    to_color.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_color.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    to_color.old_layout = sbx::graphics::image_layout::undefined;
    to_color.new_layout = sbx::graphics::image_layout::color_attachment_optimal;

    command_buffer->transition_image_layout(to_color);

    auto color_attachment = VkRenderingAttachmentInfo{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = swapchain.active_image_view();
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.clearValue.color = VkClearColorValue{{packet.clear_color.r(), packet.clear_color.g(), packet.clear_color.b(), packet.clear_color.a()}};

    auto rendering_info = VkRenderingInfo{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, extent};
    rendering_info.layerCount = 1u;
    rendering_info.colorAttachmentCount = 1u;
    rendering_info.pColorAttachments = &color_attachment;

    command_buffer->begin_rendering(rendering_info);

    // Only sample once the upload has completed (texture resident) — by then flush_writes has
    // applied the descriptor too. Until then, just show the clear.
    if (_has_texture && frame_context.timeline_value() >= _texture_frame) {
      vkCmdBindPipeline(command_buffer->handle(), _pipeline->bind_point(), *_pipeline);

      const auto descriptor_set = bindless_table.descriptor_set();
      vkCmdBindDescriptorSets(command_buffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

      const auto viewport = VkViewport{0.0f, 0.0f, static_cast<std::float_t>(extent.width), static_cast<std::float_t>(extent.height), 0.0f, 1.0f};
      command_buffer->set_viewport(viewport);

      const auto scissor = VkRect2D{VkOffset2D{0, 0}, extent};
      command_buffer->set_scissor(scissor);

      const auto values = push_constants{_texture_index, _sampler_index};

      auto range = std::array<std::byte, sbx::graphics::bindless_table::push_constant_size>{};
      std::memcpy(range.data(), &values, sizeof(push_constants));

      vkCmdPushConstants(command_buffer->handle(), bindless_table.pipeline_layout(), VK_SHADER_STAGE_ALL, 0u, static_cast<std::uint32_t>(range.size()), range.data());

      command_buffer->draw(3u, 1u, 0u, 0u);
    }

    command_buffer->end_rendering();

    auto to_present = sbx::graphics::command_buffer::image_transition_data{};
    to_present.image = swapchain.active_image();
    to_present.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_present.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    to_present.dst_stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    to_present.dst_access_mask = VK_ACCESS_2_NONE;
    to_present.old_layout = sbx::graphics::image_layout::color_attachment_optimal;
    to_present.new_layout = sbx::graphics::image_layout::present_source;

    command_buffer->transition_image_layout(to_present);
  }

  frame_context.end_frame();
}

} // namespace sbx::render
