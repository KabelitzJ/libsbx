// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_VIEW_GIZMO_HPP_
#define EDITOR_VIEWPORT_VIEW_GIZMO_HPP_

#include <imgui.h>

namespace editor {

/**
 * @brief Draws the camera-orientation cube in the viewport's top-right corner; click a face/axis to snap the view.
 *
 * Writes an updated view back onto the editor camera when clicked/dragged. Edit-mode only —
 * hidden during Play so it doesn't fight the scene's own play camera. Not tied to node
 * selection, unlike draw_viewport_gizmo.
 *
 * @param viewport_origin Screen-space top-left of the viewport image.
 * @param viewport_size Screen-space size of the viewport image.
 *
 * @return True if the cursor is over the widget — callers should skip viewport click-to-pick
 * this frame when true.
 */
auto draw_view_gizmo(const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool;

} // namespace editor

#endif // EDITOR_VIEWPORT_VIEW_GIZMO_HPP_
