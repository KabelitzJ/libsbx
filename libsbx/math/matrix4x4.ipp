// SPDX-License-Identifier: MIT
#include <libsbx/math/matrix4x4.hpp>

#include <algorithm>
#include <iomanip>
#include <cmath>

#include <libsbx/utility/assert.hpp>

namespace sbx::math {

template<scalar Type>
inline constexpr basic_matrix4x4<Type>::basic_matrix4x4(const base_type& base) noexcept
: base_type{base} { }

template<scalar Type>
template<typename Column0, typename Column1, typename Column2, typename Column3>
constexpr basic_matrix4x4<Type>::basic_matrix4x4(
  Column0&& column0,
  Column1&& column1,
  Column2&& column2,
  Column3&& column3
) noexcept
: base_type{std::forward<Column0>(column0), std::forward<Column1>(column1), std::forward<Column2>(column2), std::forward<Column3>(column3)} { }

template<scalar Type>
template<scalar Other>
inline constexpr basic_matrix4x4<Type>::basic_matrix4x4(
  Other x0, Other x1, Other x2, Other x3,
  Other y0, Other y1, Other y2, Other y3,
  Other z0, Other z1, Other z2, Other z3,
  Other w0, Other w1, Other w2, Other w3
) noexcept
: base_type{column_type{x0, y0, z0, w0}, column_type{x1, y1, z1, w1}, column_type{x2, y2, z2, w2}, column_type{x3, y3, z3, w3}} { }

template<scalar Type>
template<scalar Other>
inline constexpr basic_matrix4x4<Type>::basic_matrix4x4(const Other v00, const Other v11, const Other v22, const Other v33) noexcept
: base_type{
  column_type{v00, value_type{0}, value_type{0}, value_type{0}},
  column_type{value_type{0}, v11, value_type{0}, value_type{0}},
  column_type{value_type{0}, value_type{0}, v22, value_type{0}},
  column_type{value_type{0}, value_type{0}, value_type{0}, v33}
} { }

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::transposed(const basic_matrix4x4& matrix) noexcept -> basic_matrix4x4<Type> {
  auto result = basic_matrix4x4<value_type>{};

  result[0][0] = matrix[0][0];
  result[0][1] = matrix[1][0];
  result[0][2] = matrix[2][0];
  result[0][3] = matrix[3][0];

  result[1][0] = matrix[0][1];
  result[1][1] = matrix[1][1];
  result[1][2] = matrix[2][1];
  result[1][3] = matrix[3][1];

  result[2][0] = matrix[0][2];
  result[2][1] = matrix[1][2];
  result[2][2] = matrix[2][2];
  result[2][3] = matrix[3][2];

  result[3][0] = matrix[0][3];
  result[3][1] = matrix[1][3];
  result[3][2] = matrix[2][3];
  result[3][3] = matrix[3][3];

  return result;
}

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::inverted(const basic_matrix4x4& matrix) -> basic_matrix4x4<Type> {
  const auto coef00 = matrix[2][2] * matrix[3][3] - matrix[3][2] * matrix[2][3];
  const auto coef02 = matrix[1][2] * matrix[3][3] - matrix[3][2] * matrix[1][3];
  const auto coef03 = matrix[1][2] * matrix[2][3] - matrix[2][2] * matrix[1][3];

  const auto coef04 = matrix[2][1] * matrix[3][3] - matrix[3][1] * matrix[2][3];
  const auto coef06 = matrix[1][1] * matrix[3][3] - matrix[3][1] * matrix[1][3];
  const auto coef07 = matrix[1][1] * matrix[2][3] - matrix[2][1] * matrix[1][3];

  const auto coef08 = matrix[2][1] * matrix[3][2] - matrix[3][1] * matrix[2][2];
  const auto coef10 = matrix[1][1] * matrix[3][2] - matrix[3][1] * matrix[1][2];
  const auto coef11 = matrix[1][1] * matrix[2][2] - matrix[2][1] * matrix[1][2];

  const auto coef12 = matrix[2][0] * matrix[3][3] - matrix[3][0] * matrix[2][3];
  const auto coef14 = matrix[1][0] * matrix[3][3] - matrix[3][0] * matrix[1][3];
  const auto coef15 = matrix[1][0] * matrix[2][3] - matrix[2][0] * matrix[1][3];

  const auto coef16 = matrix[2][0] * matrix[3][2] - matrix[3][0] * matrix[2][2];
  const auto coef18 = matrix[1][0] * matrix[3][2] - matrix[3][0] * matrix[1][2];
  const auto coef19 = matrix[1][0] * matrix[2][2] - matrix[2][0] * matrix[1][2];

  const auto coef20 = matrix[2][0] * matrix[3][1] - matrix[3][0] * matrix[2][1];
  const auto coef22 = matrix[1][0] * matrix[3][1] - matrix[3][0] * matrix[1][1];
  const auto coef23 = matrix[1][0] * matrix[2][1] - matrix[2][0] * matrix[1][1];

  const auto fac0 = basic_vector4<value_type>{coef00, coef00, coef02, coef03};
  const auto fac1 = basic_vector4<value_type>{coef04, coef04, coef06, coef07};
  const auto fac2 = basic_vector4<value_type>{coef08, coef08, coef10, coef11};
  const auto fac3 = basic_vector4<value_type>{coef12, coef12, coef14, coef15};
  const auto fac4 = basic_vector4<value_type>{coef16, coef16, coef18, coef19};
  const auto fac5 = basic_vector4<value_type>{coef20, coef20, coef22, coef23};

  const auto vec0 = basic_vector4<value_type>{matrix[1][0], matrix[0][0], matrix[0][0], matrix[0][0]};
  const auto vec1 = basic_vector4<value_type>{matrix[1][1], matrix[0][1], matrix[0][1], matrix[0][1]};
  const auto vec2 = basic_vector4<value_type>{matrix[1][2], matrix[0][2], matrix[0][2], matrix[0][2]};
  const auto vec3 = basic_vector4<value_type>{matrix[1][3], matrix[0][3], matrix[0][3], matrix[0][3]};

  const auto inv0 = vec1 * fac0 - vec2 * fac1 + vec3 * fac2;
  const auto inv1 = vec0 * fac0 - vec2 * fac3 + vec3 * fac4;
  const auto inv2 = vec0 * fac1 - vec1 * fac3 + vec3 * fac5;
  const auto inv3 = vec0 * fac2 - vec1 * fac4 + vec2 * fac5;

  const auto sign0 = basic_vector4<value_type>{+1, -1, +1, -1};
  const auto sign1 = basic_vector4<value_type>{-1, +1, -1, +1};

  const auto inverse = basic_matrix4x4<value_type>{inv0 * sign0, inv1 * sign1, inv2 * sign0, inv3 * sign1};

  const auto row0 = basic_vector4<value_type>{inverse[0][0], inverse[1][0], inverse[2][0], inverse[3][0]};

  const auto det0 = matrix[0] * row0;
  
  // [NOTE] KAJ 2022-07-29 : I dont know why those parentheses are needed here... But im too scared to remove them
  const auto det1 = value_type{(det0.x() + det0.y()) + (det0.z() + det0.w())};

  const auto one_over_determinant = static_cast<value_type>(1) / det1;

  return inverse * one_over_determinant;
}

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::look_at(const basic_vector3<value_type>& position, const basic_vector3<value_type>& target, const basic_vector3<value_type>& up) noexcept -> basic_matrix4x4<Type> {
  const auto forward = basic_vector3<value_type>::normalized(target - position);
  const auto right = basic_vector3<value_type>::normalized(basic_vector3<value_type>::cross(forward, up));
  const auto new_up = basic_vector3<value_type>::cross(right, forward);

  auto result = basic_matrix4x4<value_type>::identity;

  result[0][0] = right.x();
  result[1][0] = right.y();
  result[2][0] = right.z();
  result[0][1] = new_up.x();
  result[1][1] = new_up.y();
  result[2][1] = new_up.z();
  result[0][2] = -forward.x();
  result[1][2] = -forward.y();
  result[2][2] = -forward.z();
  result[3][0] = -basic_vector3<value_type>::dot(right, position);
  result[3][1] = -basic_vector3<value_type>::dot(new_up, position);
  result[3][2] = basic_vector3<value_type>::dot(forward, position);

  return result;
}

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::perspective(const basic_angle<value_type>& fov, const value_type aspect, const value_type near, const value_type far) noexcept -> basic_matrix4x4<Type> {
  // [NOTE] KAJ 2023-11-19 : Right-handed zero-to-one depth range

  const auto tan_half_fov = std::tan(fov.to_radians() / static_cast<value_type>(2));

  auto result = basic_matrix4x4<value_type>::zero;

  result[0][0] = static_cast<value_type>(1) / (aspect * tan_half_fov);
  result[1][1] = -static_cast<value_type>(1) / tan_half_fov;
  result[2][2] = far / (near - far);
  result[2][3] = -static_cast<value_type>(1);
  result[3][2] = (near * far) / (near - far);

  //// [NOTE] KAJ 2023-10-11 : Flip the y-axis to match Vulkan's coordinate system.
  // result[1][1] *= -1;

  return result;
}

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::orthographic(const value_type left, const value_type right, const value_type bottom, const value_type top) noexcept -> basic_matrix4x4<Type> {
  auto result = basic_matrix4x4<value_type>::identity;

  result[0][0] = static_cast<value_type>(2) / (right - left);
  result[1][1] = static_cast<value_type>(2) / (top - bottom);
  result[2][2] = -static_cast<value_type>(1);
  result[3][0] = -(right + left) / (right - left);
  result[3][1] = -(top + bottom) / (top - bottom);

  return result;
}

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::orthographic(const value_type left, const value_type right, const value_type bottom, const value_type top,  const value_type near, const value_type far) noexcept -> basic_matrix4x4<Type> {
  // [NOTE] KAJ 2023-11-19 : Right-handed zero-to-one depth range

  auto result = basic_matrix4x4<value_type>::identity;

  result[0][0] = static_cast<value_type>(2) / (right - left);
  result[1][1] = -static_cast<value_type>(2) / (top - bottom);
  result[2][2] = -static_cast<value_type>(1) / (far - near);
  result[3][0] = -(right + left) / (right - left);
  result[3][1] = -(top + bottom) / (top - bottom);
  result[3][2] = -near / (far - near);

  return result;
}

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::translated(const basic_matrix4x4<Type>& matrix, const basic_vector3<typename basic_matrix4x4<Type>::value_type>& vector) noexcept -> basic_matrix4x4<Type> {
  auto result = basic_matrix4x4<value_type>{matrix};

  result[3] = matrix[0] * vector[0] + matrix[1] * vector[1] + matrix[2] * vector[2] + matrix[3];

  return result;
}

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::scaled(const basic_matrix4x4<Type>& matrix, const basic_vector3<typename basic_matrix4x4<Type>::value_type>& vector) noexcept -> basic_matrix4x4<Type> {
  auto result = basic_matrix4x4<value_type>{};

  result[0] = matrix[0] * vector[0];
  result[1] = matrix[1] * vector[1];
  result[2] = matrix[2] * vector[2];
  result[3] = matrix[3];

  return result;
}

template<scalar Type>
inline constexpr basic_matrix4x4<Type> basic_matrix4x4<Type>::rotated(const basic_matrix4x4<Type>& matrix, const basic_vector3<typename basic_matrix4x4<Type>::value_type>& axis, const basic_angle<typename basic_matrix4x4<Type>::value_type>& angle) noexcept {
  const auto radians = angle.to_radians();

  const auto cos = std::cos(radians.value());
  const auto sin = std::sin(radians.value());

  const auto normalized_axis = basic_vector3<value_type>::normalized(axis);
  const auto temp = normalized_axis * (static_cast<value_type>(1) - cos); 

  auto rotate = basic_matrix4x4<value_type>{};

  rotate[0][0] = cos + temp[0] * axis[0];
  rotate[0][1] = temp[0] * axis[1] + sin * axis[2];
  rotate[0][2] = temp[0] * axis[2] - sin * axis[1];

  rotate[1][0] = temp[1] * axis[0] - sin * axis[2];
  rotate[1][1] = cos + temp[1] * axis[1];
  rotate[1][2] = temp[1] * axis[2] + sin * axis[0];

  rotate[2][0] = temp[2] * axis[0] + sin * axis[1];
  rotate[2][1] = temp[2] * axis[1] - sin * axis[0];
  rotate[2][2] = cos + temp[2] * axis[2];

  auto result = basic_matrix4x4<value_type>{};

  result[0] = matrix[0] * rotate[0][0] + matrix[1] * rotate[0][1] + matrix[2] * rotate[0][2];
  result[1] = matrix[0] * rotate[1][0] + matrix[1] * rotate[1][1] + matrix[2] * rotate[1][2];
  result[2] = matrix[0] * rotate[2][0] + matrix[1] * rotate[2][1] + matrix[2] * rotate[2][2];
  result[3] = matrix[3];

  return result;
}

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::rotation_from_euler_angles(const basic_vector3<value_type>& euler_angles) noexcept -> basic_matrix4x4<Type> {
  const auto t1 = to_radians(basic_degree<Type>{euler_angles.x()}).value();
  const auto t2 = to_radians(basic_degree<Type>{euler_angles.y()}).value();
  const auto t3 = to_radians(basic_degree<Type>{euler_angles.z()}).value();

  const auto c1 = std::cos(-t1);
  const auto c2 = std::cos(-t2);
  const auto c3 = std::cos(-t3);
  const auto s1 = std::sin(-t1);
  const auto s2 = std::sin(-t2);
  const auto s3 = std::sin(-t3);

  auto result = basic_matrix4x4<value_type>{};

  result[0][0] = c2 * c3;
  result[0][1] = -c1 * s3 + s1 * s2 * c3;
  result[0][2] = s1 * s3 + c1 * s2 * c3;
  result[0][3] = static_cast<value_type>(0);
  result[1][0] = c2 * s3;
  result[1][1] = c1 * c3 + s1 * s2 * s3;
  result[1][2] = -s1 * c3 + c1 * s2 * s3;
  result[1][3] = static_cast<value_type>(0);
  result[2][0] = -s2;
  result[2][1] = s1 * c2;
  result[2][2] = c1 * c2;
  result[2][3] = static_cast<value_type>(0);
  result[3][0] = static_cast<value_type>(0);
  result[3][1] = static_cast<value_type>(0);
  result[3][2] = static_cast<value_type>(0);
  result[3][3] = static_cast<value_type>(1);

  return result;
}

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::operator[](size_type index) const noexcept -> const column_type& {
  return static_cast<const column_type&>(base_type::operator[](index));
}

template<scalar Type>
inline constexpr auto basic_matrix4x4<Type>::operator[](size_type index) noexcept -> column_type& {
  return static_cast<column_type&>(base_type::operator[](index));
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator+(basic_matrix4x4<Lhs> lhs, const basic_matrix4x4<Rhs>& rhs) noexcept -> basic_matrix4x4<Lhs> {
  return lhs += rhs;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator-(basic_matrix4x4<Lhs> lhs, const basic_matrix4x4<Rhs>& rhs) noexcept -> basic_matrix4x4<Lhs> {
  return lhs -= rhs;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_matrix4x4<Lhs> lhs, Rhs scalar) noexcept -> basic_matrix4x4<Lhs> {
  return lhs *= scalar;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_matrix4x4<Lhs> lhs, const basic_vector4<Rhs>& rhs) noexcept -> basic_vector4<Lhs> {
  // [NOTE] KAJ 2022-02-04 : This might become a performance bottleneck in the future. But most matrix multiplications are going to happen on the GPU anyways.
  const auto mov0 = rhs[0];
  const auto mov1 = rhs[1];

  const auto mul0 = lhs[0] * mov0;
  const auto mul1 = lhs[1] * mov1;

  const auto add0 = mul0 + mul1;

  const auto mov2 = rhs[2];
  const auto mov3 = rhs[3];

  const auto mul2 = lhs[2] * mov2;
  const auto mul3 = lhs[3] * mov3;

  const auto add1 = mul2 + mul3;

  return add0 + add1;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_matrix4x4<Lhs> lhs, const basic_matrix4x4<Rhs>& rhs) noexcept -> basic_matrix4x4<Lhs> {
  const auto lhs0 = lhs[0];
  const auto lhs1 = lhs[1];
  const auto lhs2 = lhs[2];
  const auto lhs3 = lhs[3];

  const auto rhs0 = rhs[0];
  const auto rhs1 = rhs[1];
  const auto rhs2 = rhs[2];
  const auto rhs3 = rhs[3];

  // [NOTE] KAJ 2022-02-04 : This might become a performance bottleneck in the future. But most matrix multiplications are going to happen on the GPU anyways.
  auto result = basic_matrix4x4<Lhs>{};

  result[0] = lhs0 * static_cast<Lhs>(rhs0[0]) + lhs1 * static_cast<Lhs>(rhs0[1]) + lhs2 * static_cast<Lhs>(rhs0[2]) + lhs3 * static_cast<Lhs>(rhs0[3]);
  result[1] = lhs0 * static_cast<Lhs>(rhs1[0]) + lhs1 * static_cast<Lhs>(rhs1[1]) + lhs2 * static_cast<Lhs>(rhs1[2]) + lhs3 * static_cast<Lhs>(rhs1[3]);
  result[2] = lhs0 * static_cast<Lhs>(rhs2[0]) + lhs1 * static_cast<Lhs>(rhs2[1]) + lhs2 * static_cast<Lhs>(rhs2[2]) + lhs3 * static_cast<Lhs>(rhs2[3]);
  result[3] = lhs0 * static_cast<Lhs>(rhs3[0]) + lhs1 * static_cast<Lhs>(rhs3[1]) + lhs2 * static_cast<Lhs>(rhs3[2]) + lhs3 * static_cast<Lhs>(rhs3[3]);

  return result;
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator/(basic_matrix4x4<Lhs> lhs, Rhs scalar) noexcept -> basic_matrix4x4<Lhs> {
  return lhs /= scalar;
}

} // namespace sbx::math
