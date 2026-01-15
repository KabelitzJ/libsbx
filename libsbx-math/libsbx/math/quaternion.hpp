#ifndef LIBSBX_MATH_QUATERNION_HPP_
#define LIBSBX_MATH_QUATERNION_HPP_

#include <cstddef>
#include <concepts>
#include <cmath>
#include <type_traits>

#include <yaml-cpp/yaml.h>

#include <fmt/format.h>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/algorithm.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/angle.hpp>

namespace sbx::math {

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

  template<floating_point Other = value_type>
  constexpr basic_quaternion(Other value = Other{0}) noexcept;

  template<floating_point Complex = value_type, floating_point Scalar = value_type>
  constexpr basic_quaternion(const vector_type_for<Complex>& complex, Scalar scalar) noexcept;

  template<floating_point Other = value_type>
  constexpr basic_quaternion(const vector_type_for<Other>& euler_angles) noexcept;

  template<floating_point Other = value_type>
  constexpr basic_quaternion(Other x, Other y, Other z, Other w) noexcept;

  template<floating_point Complex = value_type, floating_point Scalar = value_type>
  constexpr basic_quaternion(const vector_type_for<Complex>& axis, const basic_angle<Scalar>& angle) noexcept;

  template<floating_point Other = value_type>
  constexpr basic_quaternion(const basic_matrix4x4<Other>& matrix) noexcept;

  template<floating_point Other = value_type>
  constexpr basic_quaternion(const basic_matrix3x3<Other>& matrix) noexcept;

  template<floating_point Other = value_type>
  [[nodiscard]] static constexpr auto wxyz(Other w, Other x, Other y, Other z) noexcept -> basic_quaternion {
    return basic_quaternion{x, y, z, w};
  }

  [[nodiscard]] static constexpr auto conjugate(const basic_quaternion& quaternion) noexcept -> basic_quaternion {
    return basic_quaternion{-quaternion.complex(), quaternion.scalar()};
  }

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

  [[nodiscard]] static constexpr auto normalized(const basic_quaternion& quaternion) noexcept -> basic_quaternion {
    const auto length = quaternion.length();

		if(length <= static_cast<value_type>(0)) {
      return basic_quaternion{static_cast<value_type>(0), static_cast<value_type>(0), static_cast<value_type>(0), static_cast<value_type>(1)};
    }

		const auto one_over_length = static_cast<value_type>(1) / length;
		return basic_quaternion{quaternion.x() * one_over_length, quaternion.y() * one_over_length, quaternion.z() * one_over_length, quaternion.w() * one_over_length};
  }

  [[nodiscard]] static constexpr auto dot(const basic_quaternion& lhs, const basic_quaternion& rhs) noexcept -> value_type {
    return lhs.x() * rhs.x() + lhs.y() * rhs.y() + lhs.z() * rhs.z() + lhs.w() * rhs.w();
  }

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
   */
  [[nodiscard]] static constexpr auto slerp(const basic_quaternion& x, basic_quaternion y, const value_type a) noexcept -> basic_quaternion {
    utility::assert_that(a >= 0.0f && a <= 1.0f, "Interpolation factor out of bounds in quaternion slerp");

    auto z = y;

		auto cos_theta = dot(x, y);

		// If cos_theta < 0, the interpolation will take the long way around the sphere.
		// To fix this, one quat must be negated.
		if(cos_theta < static_cast<value_type>(0)) {
			z = -y;
			cos_theta = -cos_theta;
		}

		// Perform a linear interpolation when cos_theta is close to 1 to avoid side effect of sin(angle) becoming a zero denominator
		if(cos_theta > static_cast<value_type>(1) - math::epsilon_v<value_type>) {
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

  template<floating_point Other = value_type>
  constexpr auto operator+=(const basic_quaternion<Other>& other) noexcept -> basic_quaternion&;

  template<floating_point Other = value_type>
  constexpr auto operator-=(const basic_quaternion<Other>& other) noexcept -> basic_quaternion&;

  template<floating_point Other = value_type>
  constexpr auto operator*=(Other value) noexcept -> basic_quaternion&;

  constexpr auto operator*(const vector_type& vector) const noexcept -> vector_type {
    const auto t = vector_type::cross(_complex, vector) * 2.0f;

    return vector + (t * _scalar) + vector3::cross(_complex, t);
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

  constexpr auto normalize() noexcept -> basic_quaternion&;

private:

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
