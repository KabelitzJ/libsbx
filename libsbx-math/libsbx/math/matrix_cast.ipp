// SPDX-License-Identifier: MIT
#include <libsbx/math/matrix_cast.hpp>

namespace sbx::math {

namespace detail {

template<scalar Type>
struct matrix_cast_impl<basic_matrix3x3<Type>, basic_matrix4x4<Type>> {
  [[nodiscard]] static constexpr auto invoke(const basic_matrix4x4<Type>& matrix) -> basic_matrix3x3<Type> {
    return basic_matrix3x3<Type>{
      static_cast<Type>(matrix[0][0]), static_cast<Type>(matrix[1][0]), static_cast<Type>(matrix[2][0]),
      static_cast<Type>(matrix[0][1]), static_cast<Type>(matrix[1][1]), static_cast<Type>(matrix[2][1]),
      static_cast<Type>(matrix[0][2]), static_cast<Type>(matrix[1][2]), static_cast<Type>(matrix[2][2])
    };
  }
}; // struct matrix_cast_impl

template<scalar Type>
struct matrix_cast_impl<basic_matrix4x4<Type>, basic_matrix3x3<Type>> {
  [[nodiscard]] static constexpr auto invoke(const basic_matrix<3, 3, Type>& matrix) -> basic_matrix4x4<Type> {
    return basic_matrix4x4<Type>{
      static_cast<Type>(matrix[0][0]), static_cast<Type>(matrix[1][0]), static_cast<Type>(matrix[2][0]), static_cast<Type>(0),
      static_cast<Type>(matrix[0][1]), static_cast<Type>(matrix[1][1]), static_cast<Type>(matrix[2][1]), static_cast<Type>(0),
      static_cast<Type>(matrix[0][2]), static_cast<Type>(matrix[1][2]), static_cast<Type>(matrix[2][2]), static_cast<Type>(0),
      static_cast<Type>(0), static_cast<Type>(0), static_cast<Type>(0), static_cast<Type>(1)
    };
  }
}; // struct matrix_cast_impl

template<scalar Type>
struct matrix_cast_impl<basic_matrix4x4<Type>, basic_quaternion<Type>> {
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

}; // struct matrix_cast_impl

template<scalar Type>
struct matrix_cast_impl<basic_matrix3x3<Type>, basic_quaternion<Type>> {
  [[nodiscard]] static constexpr auto invoke(const basic_quaternion<Type>& quaternion) -> basic_matrix3x3<Type> {
    return matrix_cast_impl<basic_matrix3x3<Type>,basic_matrix4x4<Type>>::invoke(matrix_cast_impl<basic_matrix4x4<Type>, basic_quaternion<Type> >::invoke(quaternion));
  }

}; // struct matrix_cast_impl

} // namespace detail

template<typename To, typename From>
requires (dispatcher_for<detail::matrix_cast_impl<To, std::remove_cvref_t<From>>, To, From>)
[[nodiscard]] constexpr auto matrix_cast(const From& from) -> To {
  return detail::matrix_cast_impl<To, std::remove_cvref_t<From>>::invoke(from);
}

} // namespace sbx::math
