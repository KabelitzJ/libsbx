// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_TRANSFORM_GIZMO_HPP_
#define EDITOR_VIEWPORT_TRANSFORM_GIZMO_HPP_

#include <imgui.h>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws an ImGuizmo gizmo over the selection and writes drags back into local_transform.
 *
 * With 2+ nodes it manipulates a virtual pivot (average position; identity rotation in World mode, the primary node's in Local mode) and applies the rigid delta to every node.
 * Call while the Viewport window is current; 1/2/3 switch translate/rotate/scale while it's hovered.
 *
 * @param state The editor state.
 * @param viewport_origin Screen-space top-left of the viewport image.
 * @param viewport_size Screen-space size of the viewport image.
 *
 * @return True if the cursor is over the gizmo or dragging it, so viewport picking should be skipped.
 */
auto draw_viewport_gizmo(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool;

/**
 * @brief Draws translate/rotate/scale buttons over the viewport's top-left corner when a node is selected. Call after draw_viewport_gizmo.
 *
 * @param state The editor state.
 * @param viewport_origin Screen-space top-left of the viewport image.
 *
 * @return True if the cursor is over the toolbar, so viewport picking should be skipped.
 */
auto draw_gizmo_toolbar(editor_state& state, const ImVec2& viewport_origin) -> bool;

} // namespace editor

#endif // EDITOR_VIEWPORT_TRANSFORM_GIZMO_HPP_
