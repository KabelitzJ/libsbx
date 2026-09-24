// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MATH_PLANE_HPP_
#define LIBSBX_MATH_PLANE_HPP_

#include <optional>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/ray.hpp>

namespace sbx::math {

/**
 * @brief A plane in normal-distance form: a point p lies on the plane when dot(normal, p) + distance == 0.
 *
 * @tparam Type Scalar value type.
 */
template<scalar Type>
class basic_plane {

public:

  using value_type = Type;
  using vector_type = basic_vector3<value_type>;

  basic_plane() noexcept = default;

  basic_plane(const vector_type& normal, const value_type distance) noexcept
  : _normal{normal},
    _distance{distance} { }

  /** @brief Constructs from a vector4, taking .xyz() as the normal and .w() as the distance. */
  basic_plane(const basic_vector4<value_type>& plane) noexcept
  : _normal{plane.x(), plane.y(), plane.z()},
    _distance{plane.w()} { }

  auto normal() const noexcept -> const vector_type& {
    return _normal;
  }

  auto distance() const noexcept -> value_type {
    return _distance;
  }

  /** @return point's signed distance from the plane; positive on the side normal points toward. */
  auto distance_to_point(const vector_type& point) const noexcept -> value_type {
    return math::vector3::dot(_normal, point) + _distance;
  }

  /** @brief Normalizes this plane in place (unit normal, distance rescaled to match). */
  auto normalize() noexcept -> basic_plane& {
    const auto length = _normal.length();

    _normal /= length;
    _distance /= length;

    return *this;
  }

  /** @return A normalized copy of plane. */
  static auto normalized(const basic_plane& plane) noexcept -> basic_plane {
    const auto length = plane._normal.length();

    return basic_plane{plane._normal / length, plane._distance / length};
  }

  /**
   * @brief Intersects ray with this plane.
   *
   * @param ray The ray to test.
   *
   * @return The intersection point, or std::nullopt if ray is parallel to the plane or the intersection is behind ray's origin.
   */
  auto ray_intersect(const sbx::math::ray& ray) const -> std::optional<sbx::math::vector3> {
    const auto denominator = sbx::math::vector3::dot(_normal, ray.direction());

    if (sbx::math::comparison_traits<std::float_t>::equal(denominator, 0.0f)) {
      return std::nullopt;
    }

    const auto t = -distance_to_point(ray.origin()) / denominator;

    if (t < 0.0f) {
      return std::nullopt;
    }

    return ray.point_at(t);
  }

private:

  vector_type _normal;
  value_type _distance;

}; // class basic_plane

using planef = basic_plane<std::float_t>;

using plane = planef;

} // namespace sbx::math

#endif // LIBSBX_MATH_PLANE_HPP_
