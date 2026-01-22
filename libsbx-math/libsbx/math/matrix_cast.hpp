// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_MATRIX_CAST_HPP_
#define LIBSBX_MATH_MATRIX_CAST_HPP_

#include <type_traits>
#include <concepts>
#include <utility>

#include <libsbx/math/fwd.hpp>
#include <libsbx/math/concepts.hpp>
#include <libsbx/math/matrix.hpp>
#include <libsbx/math/matrix3x3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/quaternion.hpp>

namespace sbx::math {

template<typename Type, typename Return, typename... Args>
concept dispatcher_for = requires() {
  { std::remove_cvref_t<Type>::invoke(std::declval<Args>()...) } -> std::same_as<Return>;
}; // concept dispatcher_for

template<typename Type, typename Fallback, typename = void>
struct value_type_or { 
  using type = float; 
};

template<typename Type, typename Fallback>
struct value_type_or<Type, Fallback, std::void_t<typename std::remove_cvref_t<Type>::value_type>> {
  using type = typename std::remove_cvref_t<Type>::value_type;
};

template<typename Type, typename Fallback>
using value_type_or_t = typename value_type_or<Type, Fallback>::type;

template<typename Type>
using value_type_t = value_type_or_t<Type, std::float_t>;

namespace detail {

template<scalar Type>
struct matrix_cast_impl<3, 3, basic_matrix4x4<Type>> {
  [[nodiscard]] static constexpr auto invoke(const basic_matrix4x4<Type>& matrix) -> basic_matrix3x3<Type> {
    return basic_matrix3x3<Type>{
      static_cast<Type>(matrix[0][0]), static_cast<Type>(matrix[1][0]), static_cast<Type>(matrix[2][0]),
      static_cast<Type>(matrix[0][1]), static_cast<Type>(matrix[1][1]), static_cast<Type>(matrix[2][1]),
      static_cast<Type>(matrix[0][2]), static_cast<Type>(matrix[1][2]), static_cast<Type>(matrix[2][2])
    };
  }
};

template<scalar Type>
struct matrix_cast_impl<4, 4, basic_matrix3x3<Type>> {
  [[nodiscard]] static constexpr auto invoke(const basic_matrix<3, 3, Type>& matrix) -> basic_matrix4x4<Type> {
    return basic_matrix4x4<Type>{
      static_cast<Type>(matrix[0][0]), static_cast<Type>(matrix[1][0]), static_cast<Type>(matrix[2][0]), static_cast<Type>(0),
      static_cast<Type>(matrix[0][1]), static_cast<Type>(matrix[1][1]), static_cast<Type>(matrix[2][1]), static_cast<Type>(0),
      static_cast<Type>(matrix[0][2]), static_cast<Type>(matrix[1][2]), static_cast<Type>(matrix[2][2]), static_cast<Type>(0),
       static_cast<Type>(0),  static_cast<Type>(0), static_cast<Type>(0), static_cast<Type>(1),
    };
  }
};

template<scalar Type>
struct matrix_cast_impl<4, 4, basic_quaternion<Type>> {
  [[nodiscard]] static constexpr auto invoke(const basic_quaternion<Type>& quaternion) -> basic_matrix4x4<Type> {
    auto matrix = basic_matrix4x4<Type>::identity;

    const auto xx = quaternion.x() * quaternion.x();
    const auto yy = quaternion.y() * quaternion.y();
    const auto zz = quaternion.z() * quaternion.z();
    const auto xy = quaternion.x() * quaternion.y();
    const auto xz = quaternion.x() * quaternion.z();
    const auto yz = quaternion.y() * quaternion.z();
    const auto wx = quaternion.w() * quaternion.x();
    const auto wy = quaternion.w() * quaternion.y();
    const auto wz = quaternion.w() * quaternion.z();

    matrix[0][0] = 1.0f - 2.0f * (yy + zz);
    matrix[0][1] = 2.0f * (xy + wz);
    matrix[0][2] = 2.0f * (xz - wy);

    matrix[1][0] = 2.0f * (xy - wz);
    matrix[1][1] = 1.0f - 2.0f * (xx + zz);
    matrix[1][2] = 2.0f * (yz + wx);

    matrix[2][0] = 2.0f * (xz + wy);
    matrix[2][1] = 2.0f * (yz - wx);
    matrix[2][2] = 1.0f - 2.0f * (xx + yy);

    return matrix;
  }
};

template<scalar Type>
struct matrix_cast_impl<3, 3, basic_quaternion<Type>> {
  [[nodiscard]] static constexpr auto invoke(const basic_quaternion<Type>& quaternion) -> basic_matrix3x3<Type> {
    return detail::matrix_cast_impl<3, 3, basic_matrix4x4<Type>>::invoke(detail::matrix_cast_impl<4, 4, basic_quaternion<Type>>::invoke(quaternion));
  }
};

// template<>
// struct matrix_cast_impl<4, 4, transform> {
//   [[nodiscard]] static constexpr auto invoke(const transform& transform) -> matrix4x4 {
//     const auto translation = matrix4x4::translated(matrix4x4::identity, transform._position);
//     const auto scale = matrix4x4::scaled(matrix4x4::identity, transform._scale);

//     return translation *transform. _rotation_matrix * scale;
//   }
// };

} // namespace detail

template<std::size_t Columns, std::size_t Rows, typename From>
requires (dispatcher_for<detail::matrix_cast_impl<Columns, Rows, std::remove_cvref_t<From>>, concrete_matrix_t<Columns, Rows, value_type_t<From>>, From>)
[[nodiscard]] constexpr auto matrix_cast(const From& from) -> concrete_matrix_t<Columns, Rows, value_type_t<From>> {
  return detail::matrix_cast_impl<Columns, Rows, std::remove_cvref_t<From>>::invoke(from);
}

struct decompose_result {
  vector3 position;
  quaternion rotation;
  vector3 scale;
}; // struct decompose_result

[[nodiscard]] constexpr auto decompose(const matrix4x4& matrix) noexcept -> decompose_result {
  auto result = decompose_result{};

  // Extract translation
  result.position = vector3{matrix[3][0], matrix[3][1], matrix[3][2]};

  // Extract scale factors
  result.scale.x() = vector3{matrix[0][0], matrix[0][1], matrix[0][2]}.length();
  result.scale.y() = vector3{matrix[1][0], matrix[1][1], matrix[1][2]}.length();
  result.scale.z() = vector3{matrix[2][0], matrix[2][1], matrix[2][2]}.length();

  // Normalize the rotation part of the matrix
  auto rotation_matrix = matrix4x4{matrix};
  rotation_matrix[0][0] /= result.scale.x();
  rotation_matrix[0][1] /= result.scale.x();
  rotation_matrix[0][2] /= result.scale.x();

  rotation_matrix[1][0] /= result.scale.y();
  rotation_matrix[1][1] /= result.scale.y();
  rotation_matrix[1][2] /= result.scale.y();

  rotation_matrix[2][0] /= result.scale.z();
  rotation_matrix[2][1] /= result.scale.z();
  rotation_matrix[2][2] /= result.scale.z();

  result.rotation = quaternion{rotation_matrix};

  return result;
}

} // namespace sbx::math

#endif // LIBSBX_MATH_MATRIX_CAST_HPP_