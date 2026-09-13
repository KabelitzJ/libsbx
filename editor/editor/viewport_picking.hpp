// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_PICKING_HPP_
#define EDITOR_VIEWPORT_PICKING_HPP_

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Picks the scene node under a viewport-relative pixel position and selects it in @p state.
 *
 * Casts a ray from the active camera through the clicked pixel and tests it against every mesh
 * renderer's world-space bounds, keeping the nearest hit. Plain click replaces the selection with
 * the hit (or clears it on a miss); Ctrl toggles the hit node in/out of the selection; Shift adds
 * it (never removes) — Ctrl/Shift held on a miss leaves the selection untouched.
 */
auto pick_node_at_viewport_position(editor_state& state, const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> void;

/** @brief visible is false (position unspecified) for a world position behind the camera or outside the frustum. */
struct viewport_projection {
  bool visible{false};
  sbx::math::vector2 position{}; // viewport-relative pixels, only meaningful when visible
}; // struct viewport_projection

/**
 * @brief The forward counterpart to ray_from_viewport_position: projects a world position down to
 * a viewport-relative pixel position through the same view/projection @p view_projection already
 * combines (pass compute_viewport_camera_matrices's projection * view).
 */
auto project_to_viewport_position(const sbx::math::matrix4x4& view_projection, const sbx::math::vector3& world_position, const sbx::math::vector2u& viewport_size) -> viewport_projection;

} // namespace editor

#endif // EDITOR_VIEWPORT_PICKING_HPP_
