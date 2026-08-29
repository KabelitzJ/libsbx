// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/presentation_module.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/window.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/profiler.hpp>

namespace sbx::render {

presentation_module::presentation_module() {
  _render_thread = std::make_unique<render::render_thread>(
    core::engine::config().threading,
    [this]() { _consume(); }
  );

  _render_thread->run();
}

presentation_module::~presentation_module() {
  _render_thread->terminate();
}

auto presentation_module::render() -> void {
  SBX_PROFILE_SCOPE("presentation_module::render");

  auto& platform_module = core::engine::get_module<platform::platform_module>();
  auto& window = platform_module.window();

  if (window.is_iconified()) {
    return;
  }

  _render_thread->block_until_render_complete();
  _render_thread->next_frame();

  if (_ui_renderer) {
    _ui_data = _ui_renderer->build_frame();
  }

  if (_scene_renderer) {
    _scene_renderer->prepare();
  }

  _render_thread->kick();
}

auto presentation_module::set_scene_renderer(memory::observer_ptr<scene_renderer> renderer) -> void {
  _scene_renderer = renderer;
}

auto presentation_module::set_ui_renderer(memory::observer_ptr<ui_renderer> renderer) -> void {
  _ui_renderer = renderer;
}

auto presentation_module::set_compositor(std::unique_ptr<compositor> compositor) -> void {
  _compositor = std::move(compositor);
}

auto presentation_module::_consume() -> void {
  SBX_PROFILE_SCOPE("presentation_module::consume");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& frame_context = graphics_module.frame_context();

  auto command_buffer = frame_context.begin_frame();

  if (!command_buffer) {
    return;
  }

  {
    SBX_PROFILE_GPU_SCOPE((*command_buffer), "presentation_module::render");

    const auto& swapchain = frame_context.swapchain();
    const auto swapchain_extent = swapchain.extent();

    // Sole owner of the swapchain transitions: acquire -> color attachment -> present source.
    auto to_color = graphics::command_buffer::image_transition_data{};
    to_color.image = swapchain.active_image();
    to_color.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_color.src_access_mask = VK_ACCESS_2_NONE;
    to_color.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_color.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    to_color.old_layout = graphics::image_layout::undefined;
    to_color.new_layout = graphics::image_layout::color_attachment_optimal;
    command_buffer->transition_image_layout(to_color);

    if (_scene_renderer) {
      _scene_renderer->record(*command_buffer, swapchain_extent);
    }

    // Always runs, scene or not — presenting something to the swapchain is never optional.
    if (_compositor) {
      auto context = compositor_context{
        .command_buffer = command_buffer,
        .swapchain_view = swapchain.active_image_view(),
        .swapchain_extent = swapchain_extent
      };

      _compositor->execute(context);
    } else {
      clear_swapchain(*command_buffer, swapchain.active_image_view(), swapchain_extent);
    }

    // Always drawn on top of whatever the compositor wrote.
    if (_ui_renderer) {
      _ui_renderer->render(*command_buffer, swapchain_extent, _ui_data);
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

} // namespace sbx::render
