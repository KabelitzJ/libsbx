// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/epa.hpp
 *
 * @brief The Expanding Polytope Algorithm: given a GJK terminal simplex (a tetrahedron enclosing
 * the origin in the Minkowski difference A - B), expands it toward the origin's nearest boundary
 * to recover the actual penetration depth, normal, and world-space contact points.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_EPA_HPP_
#define LIBSBX_PHYSICS_EPA_HPP_

#include <libsbx/math/vector3.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/physics/gjk.hpp>
#include <libsbx/physics/shapes.hpp>

namespace sbx::physics {

struct epa_result {
  bool valid{false};
  math::vector3 normal{math::vector3::up};   // world space, points from A into B
  std::float_t penetration_depth{0.0f};
  math::vector3 point_on_a{math::vector3::zero};
  math::vector3 point_on_b{math::vector3::zero};
}; // struct epa_result

/**
 * @brief Recovers penetration depth/normal/witness points for two overlapping shapes, seeded from
 * @p gjk_simplex (the terminal tetrahedron @ref gjk_intersect produced for the same pair/pose).
 */
[[nodiscard]] auto epa_penetration(
  const convex_shape& a, const transform& pose_a,
  const convex_shape& b, const transform& pose_b,
  containers::static_vector<support_point, gjk_max_simplex_points> gjk_simplex
) -> epa_result;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_EPA_HPP_
