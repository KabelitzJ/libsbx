// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/math/ray.hpp
 *
 * @brief A 3D ray type with normalized direction for geometric intersection and sampling queries.
 *
 * @ingroup libsbx-math
 */

#ifndef LIBSBX_MATH_RAY_HPP_
#define LIBSBX_MATH_RAY_HPP_

#include <libsbx/math/vector3.hpp>

namespace sbx::math {

/**
 * @brief 3D ray with normalized direction.
 *
 * @details
 * 
 * A ray is defined by an origin and a direction. The direction is normalized
 * upon construction to guarantee a unit-length direction vector.
 */
class ray {

public:

  /**
   * @brief Constructs a default ray.
   *
   * @return Ray with origin at zero and direction pointing forward.
   */
  ray();

  /**
   * @brief Constructs a ray from an origin and a direction.
   *
   * @param origin Ray origin.
   * @param direction Ray direction (normalized internally).
   *
   * @return Constructed ray.
   */
  ray(const vector3& origin, const vector3& direction);

  /**
   * @brief Returns the ray origin.
   *
   * @return Reference to the origin.
   */
  auto origin() const -> const vector3&;

  /**
   * @brief Returns the ray direction.
   *
   * @return Reference to the normalized direction.
   */
  auto direction() const -> const vector3&;

  /**
   * @brief Computes a point along the ray at parameter t.
   *
   * @param t Ray parameter.
   *
   * @return Point at origin + direction * t.
   */
  auto point_at(const std::float_t t) const -> vector3;

private:

  vector3 _origin;
  vector3 _direction;

}; // class ray

} // namespace sbx::math

#endif // LIBSBX_MATH_RAY_HPP_
