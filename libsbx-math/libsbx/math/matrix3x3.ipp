// SPDX-License-Identifier: MIT
#include <libsbx/math/matrix3x3.hpp>

#include <algorithm>
#include <iomanip>
#include <cmath>

#include <libsbx/utility/assert.hpp>

namespace sbx::math {

template<scalar Type>
inline constexpr basic_matrix3x3<Type>::basic_matrix3x3(const base_type& base) noexcept
: base_type{base} { }

template<scalar Type>
template<typename Column0, typename Column1, typename Column2>
inline constexpr basic_matrix3x3<Type>::basic_matrix3x3(
  Column0&& column0,
  Column1&& column1,
  Column2&& column2
) noexcept
: base_type{std::forward<Column0>(column0), std::forward<Column1>(column1), std::forward<Column2>(column2)} { }

template<scalar Type>
template<scalar Other>
inline constexpr basic_matrix3x3<Type>::basic_matrix3x3(
  Other x0, Other x1, Other x2,
  Other y0, Other y1, Other y2,
  Other z0, Other z1, Other z2
) noexcept
: base_type{column_type{x0, y0, z0}, column_type{x1, y1, z1}, column_type{x2, y2, z2}} { }

template<scalar Type>
template<scalar Other>
inline constexpr basic_matrix3x3<Type>::basic_matrix3x3(const Other v00, const Other v11, const Other v22) noexcept
: base_type{
  column_type{v00, value_type{0}, value_type{0}},
  column_type{value_type{0}, v11, value_type{0}},
  column_type{value_type{0}, value_type{0}, v22}
} { }

template<scalar Type>
template<scalar Other>
constexpr basic_matrix3x3<Type>::basic_matrix3x3(const Other diagonal) noexcept
: basic_matrix3x3{diagonal, diagonal, diagonal} { }


template<scalar Type>
inline constexpr auto basic_matrix3x3<Type>::operator[](size_type index) const noexcept -> const column_type& {
  return static_cast<const column_type&>(base_type::operator[](index));
}

template<scalar Type>
inline constexpr auto basic_matrix3x3<Type>::operator[](size_type index) noexcept -> column_type& {
  return static_cast<column_type&>(base_type::operator[](index));
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_matrix3x3<Lhs> lhs, const basic_vector3<Rhs>& rhs) noexcept -> basic_vector3<Lhs> {
  return basic_vector3<Lhs>{lhs[0] * basic_vector3<Rhs>::splat_x(rhs) + lhs[1] * basic_vector3<Rhs>::splat_y(rhs) + lhs[2] * basic_vector3<Rhs>::splat_z(rhs)};
}

template<scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_matrix3x3<Lhs> lhs, const basic_matrix3x3<Rhs>& rhs) noexcept -> basic_matrix3x3<Lhs> {
  auto result = basic_matrix3x3<Lhs>{};

  for (auto column = 0u; column < 3u; ++column) {
    for (auto row = 0u; row < 3u; ++row) {
      result[column][row] = lhs[0][row] * rhs[column][0] + lhs[1][row] * rhs[column][1] + lhs[2][row] * rhs[column][2];
    }
  }

  return result;
}

}; // namespace sbx::math
