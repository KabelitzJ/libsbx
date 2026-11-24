// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_VIEW_GIZMO_HPP_
#define EDITOR_VIEWPORT_VIEW_GIZMO_HPP_

#include <imgui.h>

#include <libsbx/math/vector3.hpp>

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

/**
 * @brief Animates the editor camera to look at the origin from @p look_from_direction, as clicking a view gizmo handle does; used by the numpad view actions.
 *
 * @param look_from_direction The direction from the origin to the camera, e.g. +Z for the front view.
 */
auto request_view_snap(const sbx::math::vector3& look_from_direction) -> void;

/** @brief Points the editor camera at the world origin from its current direction and distance (Godot's Focus Origin). */
auto focus_origin() -> void;

} // namespace editor

#endif // EDITOR_VIEWPORT_VIEW_GIZMO_HPP_
