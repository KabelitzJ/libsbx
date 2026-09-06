// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/raycast.hpp
 *
 * @brief Closed-form ray-vs-convex-primitive and ray-vs-heightfield intersection, shared by
 * physics_module::raycast()'s two candidate kinds (see physics_module.hpp). Kept separate from
 * gjk.hpp/narrowphase.hpp -- this is ordinary analytic geometry, not GJK/EPA machinery.
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

namespace sbx::physics {

/** @brief A single ray intersection: world-space distance along the ray, hit point, and outward surface normal. */
struct shape_raycast_hit {
  std::float_t distance{0.0f};
  math::vector3 point{};
  math::vector3 normal{};
}; // struct shape_raycast_hit

/**
 * @brief Ray-vs-convex-primitive intersection, closed form per shape, tested in the shape's own
 * local frame (the ray is transformed by @p pose's inverse first, honoring a full per-axis scale
 * the same way find_furthest_point does). Covers every primitive a shape_collider can actually
 * carry -- sphere/cylinder/capsule/box; triangle/convex_hull are internal-only to mesh_collider
 * narrowphase and never authored directly (see shapes.hpp), so passing one here always returns
 * nullopt -- a mesh_collider raycast is a separate, larger feature (a triangle-BVH walk) not
 * attempted by this function.
 */
[[nodiscard]] auto raycast_convex_shape(const convex_shape& shape, const transform& pose, const math::ray& world_ray, std::float_t max_distance) -> std::optional<shape_raycast_hit>;

/**
 * @brief Ray-vs-heightfield intersection by marching along the ray in fixed steps (half the
 * heightmap's own cell_size, small enough not to tunnel through any slope the grid itself can
 * represent) and bisecting once a sign change in "height above terrain" is found. Outside the
 * heightmap's own XZ footprint, heightmap::sample_bilinear clamps to its edge height -- so this
 * treats the terrain as extending infinitely at its edge height beyond the mapped area, the same
 * flat-ground fallback the road system's placement spec assumes.
 */
[[nodiscard]] auto raycast_heightfield(const terrain::heightmap& map, const math::ray& world_ray, std::float_t max_distance) -> std::optional<shape_raycast_hit>;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_RAYCAST_HPP_
