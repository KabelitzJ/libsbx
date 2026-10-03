// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/epa.hpp
 *
 * @brief The Expanding Polytope Algorithm: expands GJK's terminal simplex to the nearest boundary of A - B for penetration depth, normal and contact points.
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
 * @brief Penetration depth, normal and witness points for two overlapping shapes.
 *
 * @param a The first shape.
 * @param pose_a The first shape's pose.
 * @param b The second shape.
 * @param pose_b The second shape's pose.
 * @param gjk_simplex The terminal tetrahedron @ref gjk_intersect produced for this pair.
 *
 * @return The penetration result.
 */
[[nodiscard]] auto epa_penetration(
  const convex_shape& a, const transform& pose_a,
  const convex_shape& b, const transform& pose_b,
  containers::static_vector<support_point, gjk_max_simplex_points> gjk_simplex
) -> epa_result;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_EPA_HPP_
