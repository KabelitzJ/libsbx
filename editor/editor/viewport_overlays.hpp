// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_OVERLAYS_HPP_
#define EDITOR_VIEWPORT_OVERLAYS_HPP_

#include <imgui.h>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws a clickable icon at the projected screen position of every light and camera node; clicking one selects it (Ctrl toggles, Shift adds), same modifier behavior as viewport ray-pick.
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

#endif // EDITOR_VIEWPORT_OVERLAYS_HPP_
