// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_WINDOW_HPP_
#define EDITOR_VIEWPORT_WINDOW_HPP_

#include <libsbx/graphics/resources/sampler.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws the Viewport: final_image plus, in Edit mode, the gizmos and node icons; clicks that hit none of them go to viewport picking.
 *
 * @param state The editor state.
 * @param sampler The sampler for the viewport image.
 *
 * @return True if the mouse was over the window, for is_viewport_hovered().
 */
auto draw_viewport_window(editor_state& state, const sbx::graphics::sampler& sampler) -> bool;

} // namespace editor

#endif // EDITOR_VIEWPORT_WINDOW_HPP_
