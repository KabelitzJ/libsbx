// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_RENDERER_HPP_
#define LIBSBX_RENDER_UI_RENDERER_HPP_

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/ui/ui_draw_data.hpp>

namespace sbx::render {

/**
 * @brief The UI half of presentation_module's renderer interfaces; at most one is registered.
 *
 * build_frame() deep-copies ImGui's draw data on the main thread; render() draws it with a load op, so the compositor must already have filled the swapchain image.
 */
class ui_renderer {

public:

  virtual ~ui_renderer() = default;

  /**
   * @brief Builds the frame's UI on the main thread.
   *
   * @return The frame's draw data.
   */
  virtual auto build_frame() -> ui_draw_data = 0;

  /**
   * @brief Draws @p data on the render thread.
   *
   * @param command_buffer The command buffer to record into.
   * @param extent The swapchain extent.
   * @param data The draw data from build_frame().
   */
  virtual auto render(graphics::command_buffer& command_buffer, math::vector2u extent, const ui_draw_data& data) -> void = 0;

}; // class ui_renderer

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_RENDERER_HPP_
