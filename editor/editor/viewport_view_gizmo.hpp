// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_VIEW_GIZMO_HPP_
#define EDITOR_VIEWPORT_VIEW_GIZMO_HPP_

#include <imgui.h>

namespace editor {

/**
 * @brief The camera orientation cube in the viewport's top-right corner; clicking an axis snaps the editor camera. Edit mode only.
 *
 * @param viewport_origin Screen-space top-left of the viewport image.
 * @param viewport_size Screen-space size of the viewport image.
 *
 * @return True if the cursor is over the widget, so viewport picking should be skipped.
 */
auto draw_view_gizmo(const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool;

} // namespace editor

#endif // EDITOR_VIEWPORT_VIEW_GIZMO_HPP_
