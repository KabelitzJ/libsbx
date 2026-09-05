// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_GIZMO_HPP_
#define EDITOR_VIEWPORT_GIZMO_HPP_

#include <imgui.h>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws an ImGuizmo transform gizmo over the selected node, if any, writing drags back into its local_transform.
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

/**
 * @brief Draws a clickable icon at the projected screen position of every light and camera node; clicking one selects that node.
 *
 * Always drawn on top, not depth-tested against the scene.
 *
 * @param viewport_origin Screen-space top-left of the viewport image.
 * @param viewport_size Screen-space size of the viewport image.
 * @param gizmo_capturing_input Pass draw_viewport_gizmo's return value for this frame. A
 * selected light/camera's icon can project onto the gizmo's own center move-handle; when true,
 * icon hit-testing is skipped so the gizmo keeps input priority (the glyph itself still draws).
 *
 * @return True if the cursor is over any icon — callers should skip viewport click-to-pick
 * this frame when true.
 */
auto draw_node_icons(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size, bool gizmo_capturing_input) -> bool;

/**
 * @brief Draws the selected node's camera view frustum as wireframe lines, when it has a camera component.
 *
 * Submitted into scene_renderer_module's debug_draw accumulator, not ImGui — draws no widgets
 * and captures no input.
 *
 * @param viewport_size Screen-space size of the viewport image; its aspect ratio shapes the frustum.
 */
auto draw_camera_frustum_gizmo(editor_state& state, const ImVec2& viewport_size) -> void;

} // namespace editor

#endif // EDITOR_VIEWPORT_GIZMO_HPP_
