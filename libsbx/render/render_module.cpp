// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/render_module.hpp>

#include <array>
#include <vector>
#include <span>
#include <cstddef>

#include <libsbx/math/vector3.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/window.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/profiler.hpp>

namespace sbx::render {

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

  auto& registry = graphics_module.resource_registry();
  auto& frame_context = graphics_module.frame_context();
  auto& upload_context = graphics_module.upload_context();

  auto command_buffer = frame_context.begin_frame();

  if (!command_buffer) {
    return;
  }

  {
    SBX_PROFILE_GPU_SCOPE((*command_buffer), "render_module::render");

    // Pick up a texture posted from the main thread and turn it into a GPU image here.
    {
      auto pending = std::optional<pending_texture>{};

      {
        auto lock = std::lock_guard{_texture_mutex};
        pending.swap(_pending_texture);
      }

      if (pending) {
        if (_display_ready) {
          registry.retire(_display_texture, frame_context.frame_index());
        }

        _display_texture = registry.emplace<sbx::graphics::image>(sbx::graphics::image::create_info{
          .extent = sbx::math::vector3u{pending->width, pending->height, 1u},
          .format = pending->format,
          .usage = sbx::graphics::image_usage::transfer_destination | sbx::graphics::image_usage::transfer_source | sbx::graphics::image_usage::sampled,
          .name = "Display Texture"
        });

        const auto bytes = std::span<const std::byte>{pending->pixels.data(), pending->pixels.size()};

        upload_context.stage_image(_display_texture, bytes, sbx::graphics::image_layout::transfer_source_optimal);

        _display_ready = true;
      }
    }

    upload_context.flush(*command_buffer, frame_context.frame_index());

    const auto& swapchain = frame_context.swapchain();
    const auto extent = swapchain.extent();

    if (_display_ready) {
      auto to_transfer_dst = sbx::graphics::command_buffer::image_transition_data{};
      to_transfer_dst.image = swapchain.active_image();
      to_transfer_dst.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
      to_transfer_dst.src_access_mask = VK_ACCESS_2_NONE;
      to_transfer_dst.dst_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      to_transfer_dst.dst_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      to_transfer_dst.old_layout = sbx::graphics::image_layout::undefined;
      to_transfer_dst.new_layout = sbx::graphics::image_layout::transfer_destination_optimal;

      command_buffer->transition_image_layout(to_transfer_dst);

      const auto& texture = registry.get<sbx::graphics::image>(_display_texture);

      auto blit = VkImageBlit{};
      blit.srcSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
      blit.srcOffsets[0] = VkOffset3D{0, 0, 0};
      blit.srcOffsets[1] = VkOffset3D{static_cast<std::int32_t>(texture.extent().x()), static_cast<std::int32_t>(texture.extent().y()), 1};
      blit.dstSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
      blit.dstOffsets[0] = VkOffset3D{0, 0, 0};
      blit.dstOffsets[1] = VkOffset3D{static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height), 1};

      vkCmdBlitImage(command_buffer->handle(), texture.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain.active_image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &blit, VK_FILTER_LINEAR);

      auto to_present = sbx::graphics::command_buffer::image_transition_data{};
      to_present.image = swapchain.active_image();
      to_present.src_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      to_present.src_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      to_present.dst_stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
      to_present.dst_access_mask = VK_ACCESS_2_NONE;
      to_present.old_layout = sbx::graphics::image_layout::transfer_destination_optimal;
      to_present.new_layout = sbx::graphics::image_layout::present_source;

      command_buffer->transition_image_layout(to_present);
    } else {
      // No texture yet — clear to the packet colour.
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
  }

  frame_context.end_frame();
}

} // namespace sbx::render
