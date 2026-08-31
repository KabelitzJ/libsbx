// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/gjk.hpp
 *
 * @brief The Gilbert-Johnson-Keerthi algorithm: tests whether two convex shapes overlap by
 * searching the Minkowski difference (A - B) for the origin. Hands its terminal tetrahedron off to
 * @ref epa_penetration for the actual penetration depth/normal once overlap is confirmed.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_GJK_HPP_
#define LIBSBX_PHYSICS_GJK_HPP_

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/physics/shapes.hpp>

namespace sbx::physics {

/**
 * @brief A convex shape's pose in world space. Composed from a node's local_transform and its
 * collider's own offset/rotation before narrowphase runs.
 */
struct transform {
  math::vector3 position{math::vector3::zero};
  math::quaternion rotation{math::quaternion::identity};
}; // struct transform

/**
 * @brief One point on the Minkowski difference A - B, keeping both witness points so a terminal
 * GJK simplex (or an EPA polytope built from one) can reconstruct world-space contact points.
 */
struct support_point {
  math::vector3 point{math::vector3::zero};      // point_on_a - point_on_b, world space
  math::vector3 point_on_a{math::vector3::zero}; // world space
  math::vector3 point_on_b{math::vector3::zero}; // world space
}; // struct support_point

/**
 * @brief The furthest point on @p shape (posed by @p pose) along @p world_direction, in world space.
 */
[[nodiscard]] auto support_world(const convex_shape& shape, const transform& pose, const math::vector3& world_direction) -> math::vector3;

/**
 * @brief One point on the Minkowski difference of @p a (posed by @p pose_a) and @p b (posed by @p pose_b), along @p world_direction.
 */
[[nodiscard]] auto minkowski_support(const convex_shape& a, const transform& pose_a, const convex_shape& b, const transform& pose_b, const math::vector3& world_direction) -> support_point;

inline constexpr auto gjk_max_simplex_points = std::size_t{4};

struct gjk_result {
  bool intersecting{false};
  containers::static_vector<support_point, gjk_max_simplex_points> simplex{}; // terminal tetrahedron, valid iff intersecting
}; // struct gjk_result

/**
 * @brief Tests whether @p a and @p b overlap. On overlap, @ref gjk_result::simplex holds the
 * terminal tetrahedron enclosing the origin, ready to seed @ref epa_penetration.
 */
[[nodiscard]] auto gjk_intersect(const convex_shape& a, const transform& pose_a, const convex_shape& b, const transform& pose_b) -> gjk_result;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_GJK_HPP_
