// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_SPHERE_HPP_
#define LIBSBX_MATH_SPHERE_HPP_

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/vector3.hpp>

namespace sbx::math {

template<scalar Type>
class basic_sphere {

public:

  using value_type = Type;
  using vector_type = basic_vector3<value_type>;

  basic_sphere() noexcept = default;

  basic_sphere(const vector_type& center, const value_type radius) noexcept
  : _center{center}, 
    _radius{radius} { }

  static auto transformed(const basic_sphere& sphere, const math::matrix4x4& matrix) -> basic_sphere {
    // [NOTE] KAJ 2025-04-28 : We may need to add a scale factor to the radius here.
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
