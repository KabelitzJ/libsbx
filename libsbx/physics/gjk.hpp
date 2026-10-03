// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/gjk.hpp
 *
 * @brief Gilbert-Johnson-Keerthi overlap testing on the Minkowski difference A - B; the terminal tetrahedron seeds @ref epa_penetration.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_GJK_HPP_
#define LIBSBX_PHYSICS_GJK_HPP_

#include <cmath>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/physics/shapes.hpp>

namespace sbx::physics {

/**
 * @brief A convex shape's world pose: the node's composed transform plus the collider's offset and rotation.
 *
 * `scale` is per-axis, applied in local space before rotation. Shapes that can't represent anisotropic scale themselves still collide correctly through GJK; only the closed-form fast paths require unit scale.
 */
struct transform {
  math::vector3 position{math::vector3::zero};
  math::quaternion rotation{math::quaternion::identity};
  math::vector3 scale{math::vector3::one};
}; // struct transform

/** @brief One Minkowski difference point, keeping both witness points to reconstruct contact points. */
struct support_point {
  math::vector3 point{math::vector3::zero};      // point_on_a - point_on_b, world space
  math::vector3 point_on_a{math::vector3::zero}; // world space
  math::vector3 point_on_b{math::vector3::zero}; // world space
}; // struct support_point

/**
 * @brief The furthest point on the posed @p shape along @p world_direction.
 *
 * @param shape The shape.
 * @param pose The shape's pose.
 * @param world_direction The search direction.
 *
 * @return The world-space support point.
 */
[[nodiscard]] auto support_world(const convex_shape& shape, const transform& pose, const math::vector3& world_direction) -> math::vector3;

/**
 * @brief One Minkowski difference point of @p a and @p b along @p world_direction.
 *
 * @param a The first shape.
 * @param pose_a The first shape's pose.
 * @param b The second shape.
 * @param pose_b The second shape's pose.
 * @param world_direction The search direction.
 *
 * @return The support point with both witnesses.
 */
[[nodiscard]] auto minkowski_support(const convex_shape& a, const transform& pose_a, const convex_shape& b, const transform& pose_b, const math::vector3& world_direction) -> support_point;

inline constexpr auto gjk_max_simplex_points = std::size_t{4};

struct gjk_result {
  bool intersecting{false};
  containers::static_vector<support_point, gjk_max_simplex_points> simplex{}; // terminal tetrahedron, valid only when intersecting
}; // struct gjk_result

/**
 * @brief Tests whether @p a and @p b overlap; on overlap the result's simplex encloses the origin, ready for @ref epa_penetration.
 *
 * @param a The first shape.
 * @param pose_a The first shape's pose.
 * @param b The second shape.
 * @param pose_b The second shape's pose.
 *
 * @return The result.
 */
[[nodiscard]] auto gjk_intersect(const convex_shape& a, const transform& pose_a, const convex_shape& b, const transform& pose_b) -> gjk_result;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_GJK_HPP_
