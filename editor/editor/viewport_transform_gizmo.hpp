// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_TRANSFORM_GIZMO_HPP_
#define EDITOR_VIEWPORT_TRANSFORM_GIZMO_HPP_

#include <imgui.h>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws an ImGuizmo transform gizmo over the current selection, if any, writing drags back
 * into local_transform. With exactly one node selected the gizmo sits on that node; with 2+, it
 * manipulates a virtual pivot (average position, identity rotation in World mode or the primary
 * node's rotation in Local mode) and applies the resulting rigid delta to every selected node.
 *
 * Must be called while the Viewport window is current (between its Begin/End). W/E/R switch
 * the operation (translate/rotate/scale) while the viewport is hovered.
 *
 * @param viewport_origin Screen-space top-left of the viewport image.
 * @param viewport_size Screen-space size of the viewport image.
 *
 * @return True if the cursor is over the gizmo or it's being dragged — callers should skip
 * viewport click-to-pick this frame when true.
 */
auto draw_viewport_gizmo(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool;

/**
 * @brief Draws a small floating toolbar of translate/rotate/scale buttons over the viewport's top-left corner, as a click alternative to the 1/2/3 shortcuts.
 *
 * Only drawn when a node is selected. Must be called after draw_viewport_gizmo, while the
 * Viewport window is current.
 *
 * @param viewport_origin Screen-space top-left of the viewport image.
 *
 * @return True if the cursor is over the toolbar — callers should skip viewport click-to-pick
 * this frame when true.
 */
auto draw_gizmo_toolbar(editor_state& state, const ImVec2& viewport_origin) -> bool;

} // namespace editor

#endif // EDITOR_VIEWPORT_TRANSFORM_GIZMO_HPP_
