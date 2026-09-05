// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_RENDERER_HPP_
#define LIBSBX_RENDER_UI_RENDERER_HPP_

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/ui/ui_draw_data.hpp>

namespace sbx::render {

/**
 * @brief UI half of presentation_module's renderer interfaces; at most one registered at a time
 * (presentation_module::set_ui_renderer).
 *
 * build_frame() (main thread) deep-copies ImGui's draw data into a ui_draw_data so the render
 * thread can safely consume it later. render() submits that data with VK_ATTACHMENT_LOAD_OP_LOAD,
 * so whatever ran the compositor step must already have given the swapchain image a defined
 * background.
 */
class ui_renderer {

public:

  virtual ~ui_renderer() = default;

  /** @brief Main thread, once per frame. */
  virtual auto build_frame() -> ui_draw_data = 0;

  /** @brief Render thread (or same thread, depending on threading_policy). Draws @p data into @p command_buffer. */
  virtual auto render(graphics::command_buffer& command_buffer, math::vector2u extent, const ui_draw_data& data) -> void = 0;

}; // class ui_renderer

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_RENDERER_HPP_
