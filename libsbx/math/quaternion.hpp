// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MATH_QUATERNION_HPP_
#define LIBSBX_MATH_QUATERNION_HPP_

#include <cstddef>
#include <concepts>
#include <cmath>
#include <type_traits>

#include <yaml-cpp/yaml.h>

#include <fmt/format.h>

#include <libsbx/utility/assert.hpp>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/algorithm.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/angle.hpp>

namespace sbx::math {

/**
 * @brief A rotation represented as a unit quaternion: a vector (complex) part and a scalar (real) part, avoiding the gimbal lock and interpolation problems Euler angles have.
 *
 * @tparam Type The component type.
 */
template<floating_point Type>
class basic_quaternion {

  template<floating_point Other>
  using vector_type_for = basic_vector3<Other>;

  template<floating_point Other>
  using matrix_type_for = basic_matrix4x4<Other>;

  template<floating_point Other>
  using angle_type_for = basic_angle<Other>;

  template<floating_point Other>
  using quaternion_type_for = basic_quaternion<Other>;

public:

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using size_type = std::size_t;
  using length_type = std::float_t;
  using vector_type = vector_type_for<value_type>;
  using matrix_type = matrix_type_for<value_type>;
  using angle_type = basic_angle<value_type>;

  inline static constexpr basic_quaternion identity{vector_type::zero, value_type{1}};

  /** @brief Constructs a quaternion with every component set to value. */
  template<floating_point Other = value_type>
  constexpr basic_quaternion(Other value = Other{0}) noexcept;

  /**
   * @brief Constructs from an explicit complex (vector) and scalar part.
   *
   * @tparam Complex The complex part's component type.
   * @tparam Scalar The scalar part's type.
   *
   * @param complex The complex (x, y, z) part.
   * @param scalar The scalar (w) part.
   */
  template<floating_point Complex = value_type, floating_point Scalar = value_type>
  constexpr basic_quaternion(const vector_type_for<Complex>& complex, Scalar scalar) noexcept;

  /**
   * @brief Constructs from Euler angles, in degrees, applied in roll (x), pitch (y), yaw (z) order.
   *
   * @tparam Other The euler_angles vector's component type.
   *
   * @param euler_angles The rotation, in degrees, as (roll, pitch, yaw).
   */
  template<floating_point Other = value_type>
  constexpr basic_quaternion(const vector_type_for<Other>& euler_angles) noexcept;

  /** @brief Constructs from explicit x, y, z, w components. */
  template<floating_point Other = value_type>
  constexpr basic_quaternion(Other x, Other y, Other z, Other w) noexcept;

  /**
   * @brief Constructs a rotation of angle around axis.
   *
   * @tparam Complex The axis vector's component type.
   * @tparam Scalar The angle's component type.
   *
   * @param axis The rotation axis; does not need to already be normalized.
   * @param angle The rotation angle.
   */
  template<floating_point Complex = value_type, floating_point Scalar = value_type>
  constexpr basic_quaternion(const vector_type_for<Complex>& axis, const basic_angle<Scalar>& angle) noexcept;

  /** @brief Constructs the rotation matrix's equivalent quaternion, via the largest-diagonal-term (Shepperd/Day) method. */
  template<floating_point Other = value_type>
  constexpr basic_quaternion(const basic_matrix4x4<Other>& matrix) noexcept;

  /** @copydoc basic_quaternion(const basic_matrix4x4<Other>&) */
  template<floating_point Other = value_type>
  constexpr basic_quaternion(const basic_matrix3x3<Other>& matrix) noexcept;

  /** @brief Constructs from explicit w, x, y, z components, in that argument order. */
  template<floating_point Other = value_type>
  [[nodiscard]] static constexpr auto wxyz(Other w, Other x, Other y, Other z) noexcept -> basic_quaternion {
    return basic_quaternion{x, y, z, w};
  }

  /** @return quaternion's conjugate: its complex part negated, scalar part unchanged. */
  [[nodiscard]] static constexpr auto conjugate(const basic_quaternion& quaternion) noexcept -> basic_quaternion {
    return basic_quaternion{-quaternion.complex(), quaternion.scalar()};
  }

  /** @return quaternion's multiplicative inverse, or the identity quaternion if quaternion's squared length is below math::epsilonf. */
  [[nodiscard]] static constexpr auto inverted(const basic_quaternion& quaternion) noexcept -> basic_quaternion {
    const auto length_squared = dot(quaternion, quaternion);

    if (length_squared < math::epsilonf) {
      return basic_quaternion::identity;
    }

    const auto inverse_length_squared = 1.0f / length_squared;

    const auto complex = vector_type{quaternion.x() * inverse_length_squared, quaternion.y() * inverse_length_squared, quaternion.z() * inverse_length_squared};
    const auto scalar = quaternion.w() * inverse_length_squared;

    return basic_quaternion{-complex, scalar};
  }

  /**
   * @brief Normalizes quaternion to unit length.
   *
   * @return quaternion scaled to unit length, or the identity quaternion if its length is below math::epsilon_v<value_type>.
   *
   * @note Uses the same epsilon-based degenerate-length threshold as inverted(), rather than an exact `length <= 0` check, so a quaternion with a tiny nonzero length doesn't get divided by a near-zero value.
   */
  [[nodiscard]] static constexpr auto normalized(const basic_quaternion& quaternion) noexcept -> basic_quaternion {
    const auto length_squared = dot(quaternion, quaternion);

    if (length_squared < math::epsilon_v<value_type>) {
      return basic_quaternion{static_cast<value_type>(0), static_cast<value_type>(0), static_cast<value_type>(0), static_cast<value_type>(1)};
    }

    const auto one_over_length = static_cast<value_type>(1) / std::sqrt(length_squared);

    return basic_quaternion{quaternion.x() * one_over_length, quaternion.y() * one_over_length, quaternion.z() * one_over_length, quaternion.w() * one_over_length};
  }

  /** @return The dot product of lhs and rhs's components. */
  [[nodiscard]] static constexpr auto dot(const basic_quaternion& lhs, const basic_quaternion& rhs) noexcept -> value_type {
    return lhs.x() * rhs.x() + lhs.y() * rhs.y() + lhs.z() * rhs.z() + lhs.w() * rhs.w();
  }

  /** @brief Component-wise linear interpolation between start and end — not a rotation-aware interpolation; see slerp() for that. */
  [[nodiscard]] static constexpr auto lerp(const basic_quaternion& start, const basic_quaternion& end, const value_type t) noexcept -> basic_quaternion {
    return start * (1.0f - t) + end * t;
  }

  /**
   * @brief Spherical linear interpolation between two quaternions.
   *
   * @param start The starting quaternion.
   * @param end The ending quaternion.
   * @param t The interpolation factor [0.0f, 1.0f].
   *
   * @return A new quaternion that is the result of the spherical linear interpolation.
   *
   * @throws assertion_failure If t is outside [0, 1] (debug builds only — see utility::assert_that).
   */
  [[nodiscard]] static constexpr auto slerp(const basic_quaternion& x, basic_quaternion y, const value_type a) noexcept -> basic_quaternion {
    utility::assert_that(a >= 0.0f && a <= 1.0f, "Interpolation factor out of bounds in quaternion slerp");

    auto z = y;

    auto cos_theta = dot(x, y);

    // If cos_theta < 0, the interpolation will take the long way around the sphere.
    // To fix this, one quat must be negated.
    if (cos_theta < static_cast<value_type>(0)) {
      z = -y;
      cos_theta = -cos_theta;
    }

    // Perform a linear interpolation when cos_theta is close to 1 to avoid side effect of sin(angle) becoming a zero denominator
    if (cos_theta > static_cast<value_type>(1) - math::epsilon_v<value_type>) {
      // Linear interpolation
      return basic_quaternion::wxyz(
        math::mix(x.w(), z.w(), a),
        math::mix(x.x(), z.x(), a),
        math::mix(x.y(), z.y(), a),
        math::mix(x.z(), z.z(), a)
      );
    } else {
      // Essential Mathematics, page 467
      const auto angle = std::acos(cos_theta);
      return (std::sin((static_cast<value_type>(1) - a) * angle) * x + std::sin(a * angle) * z) / std::sin(angle);
    }
  }

  /** @return quaternion's rotation, as Euler angles in degrees (roll, pitch, yaw), the inverse of the Euler-angle constructor. */
  [[nodiscard]] static constexpr auto euler_angles(const basic_quaternion& quaternion) -> vector_type {
    const auto x = quaternion.x();
    const auto y = quaternion.y();
    const auto z = quaternion.z();
    const auto w = quaternion.w();

    const auto sinr_cosp = 2.0f * (w * x + y * z);
    const auto cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    auto roll = std::atan2(sinr_cosp, cosr_cosp);

    const auto sinp = 2.0f * (w * y - z * x);
    auto pitch = 0.0f;

    if (std::abs(sinp) >= 1.0f) {
      pitch = std::copysign(math::pi_v<value_type> / 2.0f, sinp);
    } else {
      pitch = std::asin(sinp);
    }

    const auto siny_cosp = 2.0f * (w * z + x * y);
    const auto cosy_cosp = 1.0f - 2.0f * (y * y + z * z);

    auto yaw = std::atan2(siny_cosp, cosy_cosp);

    return math::vector3{to_degrees(radian{roll}).value(), to_degrees(radian{pitch}).value(), to_degrees(radian{yaw}).value()};
  }

  /**
   * @brief The rotation orienting an object's forward (-Z) along @p direction, with its up aligned to @p up. Works for cameras and directional/spot lights (all use -Z as forward).
   *
   * @param direction The direction to look at.
   * @param up The up vector to align with.
   *
   * @return A quaternion representing the rotation to look at the given direction with the specified up vector.
   */
  [[nodiscard]] static auto look_at(const vector_type& direction, const vector_type& up = vector_type::up) noexcept -> basic_quaternion {
    const auto forward = vector_type::normalized(direction);
    const auto z = forward * static_cast<value_type>(-1); // the +Z basis column (forward is -Z)

    auto reference = vector_type::normalized(up);

    // Avoid a degenerate basis when direction is (anti)parallel to up.
    if (math::abs(vector_type::dot(forward, reference)) > static_cast<value_type>(0.9999)) {
      reference = vector_type{value_type{0}, value_type{0}, value_type{1}};
    }

    const auto x = vector_type::normalized(vector_type::cross(reference, z));
    const auto y = vector_type::cross(z, x);

    return basic_quaternion{basic_matrix3x3<value_type>{x, y, z}};
  }

  template<floating_point Other = value_type>
  constexpr auto operator+=(const basic_quaternion<Other>& other) noexcept -> basic_quaternion&;

  template<floating_point Other = value_type>
  constexpr auto operator-=(const basic_quaternion<Other>& other) noexcept -> basic_quaternion&;

  template<floating_point Other = value_type>
  constexpr auto operator*=(Other value) noexcept -> basic_quaternion&;

  /** @return direction rotated by this quaternion. */
  constexpr auto operator*(const vector_type& vector) const noexcept -> vector_type {
    const auto t = vector_type::cross(_complex, vector) * 2.0f;

    return vector + (t * _scalar) + vector_type::cross(_complex, t);
  }

  template<floating_point Other = value_type>
  constexpr auto operator*=(const basic_quaternion<Other>& other) noexcept -> basic_quaternion&;

  template<floating_point Other = value_type>
  constexpr auto operator/=(Other value) noexcept -> basic_quaternion&;

  [[nodiscard]] constexpr auto x() noexcept -> reference;

  [[nodiscard]] constexpr auto x() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto y() noexcept -> reference;

  [[nodiscard]] constexpr auto y() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto z() noexcept -> reference;

  [[nodiscard]] constexpr auto z() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto w() noexcept -> reference;

  [[nodiscard]] constexpr auto w() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto complex() noexcept -> vector_type&;

  [[nodiscard]] constexpr auto complex() const noexcept -> const vector_type&;

  [[nodiscard]] constexpr auto scalar() noexcept -> reference;

  [[nodiscard]] constexpr auto scalar() const noexcept -> const_reference;

  [[nodiscard]] constexpr auto length_squared() const noexcept -> length_type;

  [[nodiscard]] constexpr auto length() const noexcept -> length_type;

  /** @brief Normalizes this quaternion in place to unit length. Does nothing if its squared length is (near) zero. */
  constexpr auto normalize() noexcept -> basic_quaternion&;

private:

  /**
   * @brief Shared implementation for the basic_matrix4x4/basic_matrix3x3 constructors: builds the quaternion equivalent to a rotation matrix's upper-left 3x3 block, via the largest-diagonal-term (Shepperd/Day) method.
   *
   * @tparam Matrix The source matrix type; only its 3x3 upper-left block ([0..2][0..2]) is read.
   *
   * @param matrix The rotation matrix to convert.
   *
   * @throws assertion_failure If matrix's diagonal doesn't correspond to a valid rotation (debug builds only — see utility::assert_that); falls back to the identity quaternion either way.
   */
  template<typename Matrix>
  [[nodiscard]] static constexpr auto _from_rotation_matrix(const Matrix& matrix) noexcept -> basic_quaternion;

  vector_type _complex;
  value_type _scalar;

}; // class basic_quaternion

template<floating_point Lhs, floating_point Rhs>
[[nodiscard]] constexpr auto operator==(const basic_quaternion<Lhs>& lhs, const basic_quaternion<Rhs>& rhs) noexcept -> bool;

template<floating_point Lhs, floating_point Rhs>
[[nodiscard]] constexpr auto operator+(basic_quaternion<Lhs> lhs, const basic_quaternion<Rhs>& rhs) noexcept -> basic_quaternion<Lhs>;

template<floating_point Lhs, floating_point Rhs>
[[nodiscard]] constexpr auto operator-(basic_quaternion<Lhs> lhs, const basic_quaternion<Rhs>& rhs) noexcept -> basic_quaternion<Lhs>;

template<floating_point Type>
[[nodiscard]] constexpr auto operator-(basic_quaternion<Type> quaternion) noexcept -> basic_quaternion<Type>;

template<floating_point Lhs, floating_point Rhs>
[[nodiscard]] constexpr auto operator*(basic_quaternion<Lhs> lhs, Rhs rhs) noexcept -> basic_quaternion<Lhs>;

template<floating_point Lhs, floating_point Rhs>
[[nodiscard]] constexpr auto operator*(Lhs lhs, basic_quaternion<Rhs> rhs) noexcept -> basic_quaternion<Rhs>;

template<floating_point Lhs, floating_point Rhs>
[[nodiscard]] constexpr auto operator*(basic_quaternion<Lhs> lhs, const basic_quaternion<Rhs>& rhs) noexcept -> basic_quaternion<Lhs>;

template<floating_point Lhs, floating_point Rhs>
[[nodiscard]] constexpr auto operator/(basic_quaternion<Lhs> lhs, Rhs rhs) noexcept -> basic_quaternion<Lhs>;

/** @brief Type alias for a quaternion with 32 bit floating-point components. */
using quaternionf = basic_quaternion<std::float_t>;

/** @brief Type alias for quaternionf */
using quaternion = quaternionf;

} // namespace sbx::math

template<sbx::math::floating_point Type>
struct std::hash<sbx::math::basic_quaternion<Type>> {

  auto operator()(const sbx::math::basic_quaternion<Type>& quaternion) const noexcept -> std::size_t;

}; // struct std::hash

template<sbx::math::floating_point Type>
struct YAML::convert<sbx::math::basic_quaternion<Type>> {

  static auto encode(const sbx::math::basic_quaternion<Type>& quaternion) -> YAML::Node;

  static auto decode(const YAML::Node& node, sbx::math::basic_quaternion<Type>& quaternion) -> bool;

}; // struct YAML::convert

template<sbx::math::floating_point Type>
auto operator<<(YAML::Emitter& out, const sbx::math::basic_quaternion<Type>& quaternion) -> YAML::Emitter& {
  return out << YAML::convert<sbx::math::basic_quaternion<Type>>::encode(quaternion);
}

template<sbx::math::floating_point Type>
struct fmt::formatter<sbx::math::basic_quaternion<Type>> {

  template<typename ParseContext>
  constexpr auto parse(ParseContext& context) -> decltype(context.begin());

  template<typename FormatContext>
  auto format(const sbx::math::basic_quaternion<Type>& quaternion, FormatContext& context) -> decltype(context.out());

}; // struct fmt::formatter

#include <libsbx/math/quaternion.ipp>

#endif // LIBSBX_MATH_QUATERNION_HPP_
