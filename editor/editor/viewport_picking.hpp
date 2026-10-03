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
 * @brief Selects the node whose mesh bounds the ray through @p position hits first.
 *
 * A plain click replaces the selection (or clears it on a miss); Ctrl toggles and Shift adds, leaving the selection untouched on a miss.
 *
 * @param state The editor state.
 * @param position The viewport-relative pixel.
 * @param viewport_size The viewport size in pixels.
 */
auto pick_node_at_viewport_position(editor_state& state, const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> void;

struct viewport_hit {
  sbx::scenes::node node{};
  sbx::math::vector3 position{}; // where the ray entered the node's bounds
}; // struct viewport_hit

/**
 * @brief The world-space ray from the viewport camera through a viewport pixel.
 *
 * @param position The viewport-relative pixel.
 * @param viewport_size The viewport size in pixels.
 *
 * @return The ray, or nullopt without a camera.
 */
auto viewport_ray(const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> std::optional<sbx::math::ray>;

/**
 * @brief The nearest active mesh renderer whose world bounds @p ray hits, the same test click selection uses.
 *
 * @param ray The world-space ray.
 *
 * @return The hit, or nullopt.
 */
auto raycast_nodes(const sbx::math::ray& ray) -> std::optional<viewport_hit>;

/**
 * @brief Where something dropped at a viewport pixel lands: the mesh under the cursor, else the y = 0 plane, else 10 units in front of the camera.
 *
 * @param position The viewport-relative pixel.
 * @param viewport_size The viewport size in pixels.
 *
 * @return The world position.
 */
auto viewport_drop_position(const sbx::math::vector2& position, const sbx::math::vector2u& viewport_size) -> sbx::math::vector3;

/** @brief A projected position; when visible is false (behind the camera or outside the frustum) position is unspecified. */
struct viewport_projection {
  bool visible{false};
  sbx::math::vector2 position{}; // viewport-relative pixels
}; // struct viewport_projection

/**
 * @brief Projects a world position to a viewport pixel, the inverse of viewport_ray.
 *
 * @param view_projection The camera's projection * view.
 * @param world_position The world position.
 * @param viewport_size The viewport size in pixels.
 *
 * @return The projection.
 */
auto project_to_viewport_position(const sbx::math::matrix4x4& view_projection, const sbx::math::vector3& world_position, const sbx::math::vector2u& viewport_size) -> viewport_projection;

} // namespace editor

#endif // EDITOR_VIEWPORT_PICKING_HPP_
