// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_OVERLAYS_HPP_
#define EDITOR_VIEWPORT_OVERLAYS_HPP_

#include <imgui.h>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws a clickable, always-on-top icon over every light and camera; clicking selects like viewport picking (Ctrl toggles, Shift adds).
 *
 * @param state The editor state.
 * @param viewport_origin Screen-space top-left of the viewport image.
 * @param viewport_size Screen-space size of the viewport image.
 * @param gizmo_capturing_input draw_viewport_gizmo's result this frame; when true, icons still draw but skip hit-testing so the gizmo keeps priority.
 *
 * @return True if the cursor is over an icon, so viewport picking should be skipped.
 */
auto draw_node_icons(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size, bool gizmo_capturing_input) -> bool;

/**
 * @brief Draws the selected camera's view frustum through the debug_draw accumulator; no widgets, no input.
 *
 * @param state The editor state.
 * @param viewport_size Screen-space size of the viewport image; its aspect ratio shapes the frustum.
 */
auto draw_camera_frustum_gizmo(editor_state& state, const ImVec2& viewport_size) -> void;

} // namespace editor

#endif // EDITOR_VIEWPORT_OVERLAYS_HPP_
