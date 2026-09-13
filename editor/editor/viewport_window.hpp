// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_WINDOW_HPP_
#define EDITOR_VIEWPORT_WINDOW_HPP_

#include <libsbx/graphics/resources/sampler.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws the Viewport window: embeds scene_renderer_module::final_image via ImGui::Image(),
 * then (edit mode only) the transform gizmo, its toolbar, the view-orientation cube, and node-icon
 * overlays over it, and dispatches a click that lands on none of those to viewport ray-picking.
 *
 * @param sampler The Viewport image's own filtering sampler (see editor_ui_layer::_sampler's doc
 * comment) -- not a backend concern, just how this one image should be sampled.
 *
 * @return True if the mouse was over the window this frame -- callers should surface this via
 * their own is_viewport_hovered()-style accessor, since other systems (camera controllers, play
 * mode input) query it without depending on where the window itself gets drawn.
 */
auto draw_viewport_window(editor_state& state, const sbx::graphics::sampler& sampler) -> bool;

} // namespace editor

#endif // EDITOR_VIEWPORT_WINDOW_HPP_
