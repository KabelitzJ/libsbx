// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/quickhull.hpp
 *
 * @brief Exact 3D convex hull (Quickhull) of a point cloud as a triangulated vertex/face list; convex_hull_cache is its capped, cached consumer.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_QUICKHULL_HPP_
#define LIBSBX_PHYSICS_QUICKHULL_HPP_

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <libsbx/math/vector3.hpp>

namespace sbx::physics {

struct hull_face {
  std::array<std::uint32_t, 3> indices; // into hull_result::vertices, wound counter-clockwise around the outward normal
}; // struct hull_face

struct hull_result {
  std::vector<math::vector3> vertices;
  std::vector<hull_face> faces;
}; // struct hull_result

/**
 * @brief The exact convex hull of @p points. Degenerate input (under 4 independent points) returns the points with no faces; the support function still works on those.
 *
 * @param points The point cloud.
 *
 * @return The hull.
 */
[[nodiscard]] auto compute_convex_hull(std::span<const math::vector3> points) -> hull_result;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_QUICKHULL_HPP_
