// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_GIZMO_HPP_
#define EDITOR_VIEWPORT_GIZMO_HPP_

#include <imgui.h>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws an ImGuizmo transform gizmo over the selected node, if any, and writes drags back
 * into its local_transform. Must be called while the Viewport window is the current ImGui window
 * (between its Begin/End), since it draws into that window's draw list. W/E/R switch the
 * operation (translate/rotate/scale) while the viewport is hovered.
 *
 * @param viewport_origin Screen-space top-left of the viewport image.
 * @param viewport_size Screen-space size of the viewport image.
 *
 * @return true if the cursor is over the gizmo or it's currently being dragged — callers should
 * skip viewport click-to-pick this frame when true, so grabbing a handle doesn't also reselect.
 */
auto draw_viewport_gizmo(editor_state& state, const ImVec2& viewport_origin, const ImVec2& viewport_size) -> bool;

} // namespace editor

#endif // EDITOR_VIEWPORT_GIZMO_HPP_
