// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MATH_FRUSTUM_HPP_
#define LIBSBX_MATH_FRUSTUM_HPP_

#include <array>
#include <cmath>
#include <cstddef>

#include <libsbx/math/constants.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector4.hpp>

namespace sbx::math {

/**
 * @brief Extracts the 6 world-space frustum planes (left, right, bottom, top, near, far, in that order) from a combined view-projection matrix — the Gribb-Hartmann method: each plane falls straight out of a row combination of @p view_projection, no matrix inverse needed, and because @p view_projection maps world space directly to clip space in one step, the planes it yields are already in world space (not clip space). Assumes column-major storage with a column-vector convention (`clip = view_projection * point`, matching basic_matrix4x4::perspective/this engine's shaders' `mul(matrix, vector)`) and a right-handed, zero-to-one clip-space depth range (matching basic_matrix4x4::perspective's own convention).
 *
 * Each returned plane is normalized: `.xyz()` is a unit inward-facing normal (positive on the inside of the frustum) and `.w()` is the plane's signed distance term, so a point/AABB test can use `dot(normal, p) + distance >= 0` directly without any further scaling.
 */
[[nodiscard]] inline auto extract_frustum_planes(const matrix4x4& view_projection) noexcept -> std::array<vector4, 6u> {
  const auto& m = view_projection;

  // Row i of a column-major matrix is the i-th component across every column.
  const auto row = [&m](std::size_t index) -> vector4 {
    return vector4{m[0][index], m[1][index], m[2][index], m[3][index]};
  };

  const auto row0 = row(0u);
  const auto row1 = row(1u);
  const auto row2 = row(2u);
  const auto row3 = row(3u);

  auto planes = std::array<vector4, 6u>{
    row3 + row0, // left
    row3 - row0, // right
    row3 + row1, // bottom
    row3 - row1, // top
    row2,        // near (zero-to-one depth: z >= 0)
    row3 - row2  // far
  };

  for (auto& plane : planes) {
    const auto length = std::sqrt(plane.x() * plane.x() + plane.y() * plane.y() + plane.z() * plane.z());

    if (length > math::epsilonf) {
      plane = plane / length;
    }
  }

  return planes;
}

} // namespace sbx::math

#endif // LIBSBX_MATH_FRUSTUM_HPP_
