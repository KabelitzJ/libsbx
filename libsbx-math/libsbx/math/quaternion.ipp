// SPDX-License-Identifier: MIT
#include <libsbx/math/quaternion.hpp>

#include <cmath>
#include <cassert>
#include <iostream>

#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/assert.hpp>

#include <libsbx/math/angle.hpp>

namespace sbx::math {

template<floating_point Type>
template<floating_point Other>
inline constexpr basic_quaternion<Type>::basic_quaternion(Other value) noexcept
: _complex{value},
  _scalar{static_cast<value_type>(value)} { }

template<floating_point Type>
template<floating_point Complex, floating_point Scalar>
inline constexpr basic_quaternion<Type>::basic_quaternion(const vector_type_for<Complex>& complex, Scalar scalar) noexcept
: _complex{complex},
  _scalar{static_cast<value_type>(scalar)} { }

template<floating_point Type>
template<floating_point Other>
inline constexpr basic_quaternion<Type>::basic_quaternion(const vector_type_for<Other>& euler_angles) noexcept {
  // [TODO]: Need to implement
}

template<floating_point Type>
template<floating_point Other>
inline constexpr basic_quaternion<Type>::basic_quaternion(Other x, Other y, Other z, Other w) noexcept
: _complex{x, y, z},
  _scalar{static_cast<value_type>(w)} { }

template<floating_point Type>
template<floating_point Complex, floating_point Scalar>
inline constexpr basic_quaternion<Type>::basic_quaternion(const vector_type_for<Complex>& axis, const basic_angle<Scalar>& angle) noexcept {
  const auto rangle_rad = angle.to_radians();
  const auto norm_axis = vector_type_for<Complex>::normalized(axis);

  const auto w = std::cos(rangle_rad.value() / static_cast<Scalar>(2));
  const auto v = std::sin(rangle_rad.value() / static_cast<Scalar>(2));

  _complex = norm_axis * v;
  _scalar = w;
}

template<floating_point Type>
template<floating_point Other>
inline constexpr basic_quaternion<Type>::basic_quaternion(const basic_matrix4x4<Other>& matrix) noexcept {
  const auto four_x_squared_minus1 = matrix[0][0] - matrix[1][1] - matrix[2][2];
  const auto four_y_squared_minus1 = matrix[1][1] - matrix[0][0] - matrix[2][2];
  const auto four_z_squared_minus1 = matrix[2][2] - matrix[0][0] - matrix[1][1];
  const auto four_w_squared_minus1 = matrix[0][0] + matrix[1][1] + matrix[2][2];

  auto biggest_index = 0;
  auto four_biggest_squared_minus1 = four_w_squared_minus1;

  if (four_x_squared_minus1 > four_biggest_squared_minus1) {
    four_biggest_squared_minus1 = four_x_squared_minus1;
    biggest_index = 1;
  }

  if (four_y_squared_minus1 > four_biggest_squared_minus1) {
    four_biggest_squared_minus1 = four_y_squared_minus1;
    biggest_index = 2;
  }

  if(four_z_squared_minus1 > four_biggest_squared_minus1) {
    four_biggest_squared_minus1 = four_z_squared_minus1;
    biggest_index = 3;
  }

  const auto biggest_val = std::sqrt(four_biggest_squared_minus1 + static_cast<Type>(1)) * static_cast<Type>(0.5);
  const auto mult = static_cast<Type>(0.25) / biggest_val;

  switch (biggest_index) {
    case 0: {
      _complex = vector_type{(matrix[1][2] - matrix[2][1]) * mult, (matrix[2][0] - matrix[0][2]) * mult, (matrix[0][1] - matrix[1][0]) * mult};
      _scalar = biggest_val;
      break;
    }
    case 1: {
      _complex = vector_type{biggest_val, (matrix[0][1] + matrix[1][0]) * mult, (matrix[2][0] + matrix[0][2]) * mult};
      _scalar = (matrix[1][2] - matrix[2][1]) * mult;
      break;
    }
    case 2: {
      _complex = vector_type{(matrix[0][1] + matrix[1][0]) * mult, biggest_val, (matrix[1][2] + matrix[2][1]) * mult};
      _scalar = (matrix[2][0] - matrix[0][2]) * mult;
      break;
    }
    case 3: {
      _complex = vector_type{(matrix[2][0] + matrix[0][2]) * mult, (matrix[1][2] + matrix[2][1]) * mult, biggest_val};
      _scalar = (matrix[0][1] - matrix[1][0]) * mult;
      break;
    }
    default: {
      utility::assert_that(false, "Failed to create quaternion from matrix4x4");
      *this = basic_quaternion<Type>::identity;
      break;
    }
  }

  normalize();
}

template<floating_point Type>
template<floating_point Other>
constexpr basic_quaternion<Type>::basic_quaternion(const basic_matrix3x3<Other>& matrix) noexcept {
  const auto trace = static_cast<value_type>(matrix[0][0]) + static_cast<value_type>(matrix[1][1]) + static_cast<value_type>(matrix[2][2]);

  if (trace > static_cast<value_type>(0)) {
    const auto s = math::sqrt(trace + static_cast<value_type>(1)) * static_cast<value_type>(2);

    _scalar = static_cast<value_type>(0.25) * s;

    const auto x = (matrix[2][1] - matrix[1][2]) / s;
    const auto y = (matrix[0][2] - matrix[2][0]) / s;
    const auto z = (matrix[1][0] - matrix[0][1]) / s;

    _complex = vector_type{x, y, z};
  } else if (matrix[0][0] > matrix[1][1] && matrix[0][0] > matrix[2][2]) {
    const auto s = math::sqrt(static_cast<value_type>(1) + matrix[0][0] - matrix[1][1] - matrix[2][2]) * static_cast<value_type>(2);

    _scalar = (matrix[2][1] - matrix[1][2]) / s;

    const auto x = static_cast<value_type>(0.25) * s;
    const auto y = (matrix[0][1] + matrix[1][0]) / s;
    const auto z = (matrix[0][2] + matrix[2][0]) / s;

    _complex = vector_type{x, y, z};
  } else if (matrix[1][1] > matrix[2][2]) {
    const auto s = math::sqrt(static_cast<value_type>(1) + matrix[1][1] - matrix[0][0] - matrix[2][2]) * static_cast<value_type>(2);

    _scalar = (matrix[0][2] - matrix[2][0]) / s;

    const auto x = (matrix[0][1] + matrix[1][0]) / s;
    const auto y = static_cast<value_type>(0.25) * s;
    const auto z = (matrix[1][2] + matrix[2][1]) / s;

    _complex = vector_type{x, y, z};
  } else {
    const auto s = math::sqrt(static_cast<value_type>(1) + matrix[2][2] - matrix[0][0] - matrix[1][1]) * static_cast<value_type>(2);

    _scalar = (matrix[1][0] - matrix[0][1]) / s;

    const auto x = (matrix[0][2] + matrix[2][0]) / s;
    const auto y = (matrix[1][2] + matrix[2][1]) / s;
    const auto z = static_cast<value_type>(0.25) * s;

    _complex = vector_type{x, y, z};
  }

  normalize();
}

template<floating_point Type>
template<floating_point Other>
inline constexpr auto basic_quaternion<Type>::operator+=(const basic_quaternion<Other>& other) noexcept -> basic_quaternion& {
  _complex += other.complex();
  _scalar += other.scalar();

  return *this;
}

template<floating_point Type>
template<floating_point Other>
inline constexpr auto basic_quaternion<Type>::operator-=(const basic_quaternion<Other>& other) noexcept -> basic_quaternion& {
  _complex -= other.complex();
  _scalar -= other.scalar();

  return *this;
}

template<floating_point Type>
template<floating_point Other>
inline constexpr auto basic_quaternion<Type>::operator*=(Other value) noexcept -> basic_quaternion& {
  _complex *= value;
  _scalar *= value;

  return *this;
}

template<floating_point Type>
template<floating_point Other>
inline constexpr auto basic_quaternion<Type>::operator*=(const basic_quaternion<Other>& other) noexcept -> basic_quaternion& {
  const auto scalar = _scalar * other.scalar() - vector_type::dot(_complex, other.complex());
  const auto complex = _complex * other.scalar() + other.complex() * _scalar + vector_type::cross(_complex, other.complex());

  _complex = complex;
  _scalar = scalar;

  return *this;
}

template<floating_point Type>
template<floating_point Other>
inline constexpr auto basic_quaternion<Type>::operator/=(Other value) noexcept -> basic_quaternion& {
  _complex /= value;
  _scalar /= value;

  return *this;
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::x() noexcept -> reference {
  return _complex.x();
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::x() const noexcept -> const_reference {
  return _complex.x();
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::y() noexcept -> reference {
  return _complex.y();
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::y() const noexcept -> const_reference {
  return _complex.y();
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::z() noexcept -> reference {
  return _complex.z();
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::z() const noexcept -> const_reference {
  return _complex.z();
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::w() noexcept -> reference {
  return _scalar;
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::w() const noexcept -> const_reference {
  return _scalar;
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::complex() noexcept -> vector_type& {
  return _complex;
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::complex() const noexcept -> const vector_type& {
  return _complex;
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::scalar() noexcept -> reference {
  return _scalar;
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::scalar() const noexcept -> const_reference {
  return _scalar;
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::length_squared() const noexcept -> length_type {
  return _complex.length_squared() + _scalar * _scalar;
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::length() const noexcept -> length_type {
  return std::sqrt(dot(*this, *this));
}

template<floating_point Type>
inline constexpr auto basic_quaternion<Type>::normalize() noexcept -> basic_quaternion& {
  const auto length_squared = this->length_squared();

  if (!comparision_traits<length_type>::equal(length_squared, static_cast<length_type>(0))) {
    *this /= std::sqrt(length_squared);
  }

  return *this;
}

template<floating_point Lhs, floating_point Rhs>
inline constexpr auto operator==(const basic_quaternion<Lhs>& lhs, const basic_quaternion<Rhs>& rhs) noexcept -> bool {
  return lhs.complex() == rhs.complex() && comparision_traits<Lhs>::equal(lhs.scalar(), rhs.scalar());
}

template<floating_point Lhs, floating_point Rhs>
inline constexpr auto operator+(basic_quaternion<Lhs> lhs, const basic_quaternion<Rhs>& rhs) noexcept -> basic_quaternion<Lhs> {
  return lhs += rhs;
}

template<floating_point Lhs, floating_point Rhs>
inline constexpr auto operator-(basic_quaternion<Lhs> lhs, const basic_quaternion<Rhs>& rhs) noexcept -> basic_quaternion<Lhs> {
  return lhs -= rhs;
}

template<floating_point Type>
inline constexpr auto operator-(basic_quaternion<Type> quat) noexcept -> basic_quaternion<Type> {
  return basic_quaternion<Type>{-quat.complex(), -quat.scalar()};
}

template<floating_point Lhs, floating_point Rhs>
inline constexpr auto operator*(basic_quaternion<Lhs> lhs, Rhs rhs) noexcept -> basic_quaternion<Lhs> {
  return lhs *= rhs;
}

template<floating_point Lhs, floating_point Rhs>
inline constexpr auto operator*(Lhs lhs, basic_quaternion<Rhs> rhs) noexcept -> basic_quaternion<Rhs> {
  return rhs *= lhs;
}

template<floating_point Lhs, floating_point Rhs>
inline constexpr auto operator*(basic_quaternion<Lhs> lhs, const basic_quaternion<Rhs>& rhs) noexcept -> basic_quaternion<Lhs> {
  return lhs *= rhs;
}

template<floating_point Lhs, floating_point Rhs>
inline constexpr auto operator/(basic_quaternion<Lhs> lhs, Rhs rhs) noexcept -> basic_quaternion<Lhs> {
  return lhs /= rhs;
}

} // namespace sbx::math

template<sbx::math::floating_point Type>
inline auto std::hash<sbx::math::basic_quaternion<Type>>::operator()(const sbx::math::basic_quaternion<Type>& quat) const noexcept -> std::size_t {
  auto seed = std::size_t{0};

  sbx::utility::hash_combine(seed, quat.x());
  sbx::utility::hash_combine(seed, quat.y());
  sbx::utility::hash_combine(seed, quat.z());
  sbx::utility::hash_combine(seed, quat.w);

  return seed;
}

template<sbx::math::floating_point Type>
auto YAML::convert<sbx::math::basic_quaternion<Type>>::decode(const Node& node, sbx::math::basic_quaternion<Type>& quat) -> bool {
  if (!node.IsSequence() || node.size() != 4) {
    return false;
  }

  quat.x() = node[0].as<Type>();
  quat.y() = node[1].as<Type>();
  quat.z() = node[2].as<Type>();
  quat.w() = node[3].as<Type>();

  return true;
}

template<sbx::math::floating_point Type>
auto YAML::convert<sbx::math::basic_quaternion<Type>>::encode(const sbx::math::basic_quaternion<Type>& quat) -> Node {
  auto node = Node{};

  node.SetStyle(YAML::EmitterStyle::Flow);

  node["x"] = quat.x();
  node["y"] = quat.y();
  node["z"] = quat.z();
  node["w"] = quat.w();

  return node;
}

template<sbx::math::floating_point Type>
template<typename ParseContext>
inline constexpr auto fmt::formatter<sbx::math::basic_quaternion<Type>>::parse(ParseContext& context) -> decltype(context.begin()) {
  return context.begin();
}

template<sbx::math::floating_point Type>
template<typename FormatContext>
inline auto fmt::formatter<sbx::math::basic_quaternion<Type>>::format(const sbx::math::basic_quaternion<Type>& quat, FormatContext& context) -> decltype(context.out()) {
  if constexpr (sbx::math::is_floating_point_v<Type>) {
    return fmt::format_to(context.out(), "{{x: {:.2f}, y: {:.2f}, z: {:.2f}, w: {:.2f}}}", quat.x(), quat.y(), quat.z(), quat.w());
  } else {
    return fmt::format_to(context.out(), "{{x: {}, y: {}, z: {}, w: {}}}", quat.x(), quat.y(), quat.z(), quat.w());
  }
}
