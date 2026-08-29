// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_RENDERER_HPP_
#define LIBSBX_RENDER_UI_RENDERER_HPP_

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/render/ui/ui_draw_data.hpp>

namespace sbx::render {

/**
 * @brief What presentation_module calls into, at most one registered at a time
 * (presentation_module::set_ui_renderer), to have UI drawn on top of whatever the frame's
 * compositor wrote to the swapchain. ui_module is the only real implementer.
 *
 * Deliberately parallel to scene_renderer, not a special case: build_frame() is the main-thread
 * phase (ImGui::NewFrame() through ImGui::Render(), deep-copied into a ui_draw_data so a render
 * thread can safely consume a frame the main thread has already moved past — see ui_draw_data's
 * own doc comment); render() is the render-thread phase, submitting that draw data with
 * VK_ATTACHMENT_LOAD_OP_LOAD — whatever ran the compositor step is responsible for giving the
 * swapchain image a defined background first.
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
