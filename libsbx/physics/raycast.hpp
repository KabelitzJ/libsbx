// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/raycast.hpp
 *
 * @brief Closed-form ray vs convex primitive and ray vs heightfield intersection for physics_module::raycast().
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_RAYCAST_HPP_
#define LIBSBX_PHYSICS_RAYCAST_HPP_

#include <optional>

#include <libsbx/math/ray.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/terrain/heightmap.hpp>

#include <libsbx/physics/shapes.hpp>
#include <libsbx/physics/gjk.hpp>
#include <libsbx/physics/mesh_collision_cache.hpp>

namespace sbx::physics {

/** @brief One ray hit: distance along the ray, point and outward normal, in world space. */
struct shape_raycast_hit {
  std::float_t distance{0.0f};
  math::vector3 point{};
  math::vector3 normal{};
}; // struct shape_raycast_hit

/**
 * @brief Ray vs sphere, cylinder, capsule or box in the shape's local frame, honoring per-axis scale. Triangles and hulls return nullopt.
 *
 * @param shape The shape.
 * @param pose The shape's pose.
 * @param world_ray The ray.
 * @param max_distance The maximum distance.
 *
 * @return The hit, or nullopt.
 */
[[nodiscard]] auto raycast_convex_shape(const convex_shape& shape, const transform& pose, const math::ray& world_ray, std::float_t max_distance) -> std::optional<shape_raycast_hit>;

/**
 * @brief Ray vs heightfield, marching in half-cell steps and bisecting at the crossing. The terrain extends at its edge height beyond the map.
 *
 * @param map The heightmap.
 * @param world_ray The ray.
 * @param max_distance The maximum distance.
 *
 * @return The hit, or nullopt.
 */
/** @brief Nearest hit against a non-convex mesh's triangles, two-sided; the normal faces the ray's origin. */
[[nodiscard]] auto raycast_triangle_mesh(const mesh_collision_data& mesh, const transform& mesh_pose, const math::ray& world_ray, std::float_t max_distance) -> std::optional<shape_raycast_hit>;

[[nodiscard]] auto raycast_heightfield(const terrain::heightmap& map, const math::ray& world_ray, std::float_t max_distance) -> std::optional<shape_raycast_hit>;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_RAYCAST_HPP_
