// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_VIEWPORT_PICKING_HPP_
#define EDITOR_VIEWPORT_PICKING_HPP_

#include <optional>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/ray.hpp>
#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/node.hpp>

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

struct viewport_hit {
  sbx::scenes::node node{};
  sbx::math::vector3 position{}; // world-space point where the ray entered node's bounds
}; // struct viewport_hit

/** @brief World-space ray from the viewport camera through a viewport-relative pixel; nullopt if there's no camera. */
auto viewport_ray(const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> std::optional<sbx::math::ray>;

/** @brief Nearest active mesh renderer whose world bounds @p ray hits -- the same test click-selection uses. */
auto raycast_nodes(const sbx::math::ray& ray) -> std::optional<viewport_hit>;

/** @brief Where something dropped at a viewport-relative pixel should land: the mesh under the cursor, else the y = 0 ground plane, else 10 units in front of the camera. */
auto viewport_drop_position(const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> sbx::math::vector3;

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
