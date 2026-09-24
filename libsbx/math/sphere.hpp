// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MATH_SPHERE_HPP_
#define LIBSBX_MATH_SPHERE_HPP_

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/vector3.hpp>

namespace sbx::math {

/**
 * @brief A bounding sphere.
 *
 * @tparam Type Scalar value type.
 */
template<scalar Type>
class basic_sphere {

public:

  using value_type = Type;
  using vector_type = basic_vector3<value_type>;

  basic_sphere() noexcept = default;

  /**
   * @brief Constructs from a center and radius.
   *
   * @param center The sphere's center.
   * @param radius The sphere's radius.
   */
  basic_sphere(const vector_type& center, const value_type radius) noexcept
  : _center{center},
    _radius{radius} { }

  /**
   * @brief Transforms sphere's center by matrix, keeping its radius unchanged.
   *
   * @param sphere The sphere to transform.
   * @param matrix The transform to apply.
   *
   * @return The transformed sphere.
   *
   * @note Does not scale the radius. If matrix carries scale, the returned sphere's radius will
   * be too small/large for the transformed geometry it's meant to bound.
   */
  static auto transformed(const basic_sphere& sphere, const math::matrix4x4& matrix) -> basic_sphere {
    const auto transformed_center = math::vector3{matrix * math::vector4{sphere._center, 1.0f}};
    return basic_sphere{transformed_center, sphere._radius};
  }

  auto center() const noexcept -> const vector_type& {
    return _center;
  }

  auto radius() const noexcept -> value_type {
    return _radius;
  }

private:

  vector_type _center;
  value_type _radius;

}; // class basic_sphere

using spheref = basic_sphere<std::float_t>;

using sphere = spheref;

} // namespace sbx::math

#endif // LIBSBX_MATH_SPHERE_HPP_
