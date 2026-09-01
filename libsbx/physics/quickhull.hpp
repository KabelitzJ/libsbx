// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/quickhull.hpp
 *
 * @brief Incremental 3-D convex hull construction (Barber/Dobkin/Huhdanpaa's "Quickhull" family):
 * given an arbitrary point cloud, returns the exact convex hull as a triangulated vertex/face list.
 * General-purpose and unbounded -- convex_hull_cache.hpp is the mesh-specific, capped/cached
 * consumer of this.
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
  std::array<std::uint32_t, 3> indices; // into hull_result::vertices, wound so the face's outward normal follows the right-hand rule
}; // struct hull_face

struct hull_result {
  std::vector<math::vector3> vertices;
  std::vector<hull_face> faces;
}; // struct hull_result

/**
 * @brief Computes the exact convex hull of @p points. Degenerates gracefully for inputs with fewer
 * than 4 points, or fewer than 4 affinely-independent ones (all coincident/collinear/coplanar):
 * returns whatever points survive as vertices and no faces, rather than failing -- callers that only
 * need a support function (GJK/EPA) still work correctly on a point-only result, they just don't get
 * a wireframe to draw.
 */
[[nodiscard]] auto compute_convex_hull(std::span<const math::vector3> points) -> hull_result;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_QUICKHULL_HPP_
