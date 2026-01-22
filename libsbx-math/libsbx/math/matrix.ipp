// SPDX-License-Identifier: MIT
#include <libsbx/math/matrix.hpp>

#include <libsbx/utility/make_array.hpp>

namespace sbx::math {

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
inline constexpr basic_matrix<Columns, Rows, Type>::basic_matrix() noexcept
: _columns{utility::make_array<column_type, Columns>(static_cast<value_type>(0))} { }

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
template<scalar Other>
inline constexpr basic_matrix<Columns, Rows, Type>::basic_matrix(const Other value) noexcept
: basic_matrix{identity(value)} { }

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
template<scalar Other>
inline constexpr basic_matrix<Columns, Rows, Type>::basic_matrix(const basic_matrix<Columns, Rows, Other>& other) noexcept
: _columns{utility::make_array<column_type, Columns>(other._columns)} { }

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
inline constexpr auto basic_matrix<Columns, Rows, Type>::operator[](size_type index) noexcept -> column_type& {
  return _columns[index];
}

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
inline constexpr auto basic_matrix<Columns, Rows, Type>::operator[](size_type index) const noexcept -> const column_type& {
  return _columns[index];
}

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
template<scalar Other>
inline constexpr auto basic_matrix<Columns, Rows, Type>::operator+=(const basic_matrix<Columns, Rows, Other>& other) noexcept -> basic_matrix& {
  for (auto i : std::views::iota(0u, Columns)) {
    _columns[i] += other[i];
  }

  return *this;
}

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
template<scalar Other>
inline constexpr auto basic_matrix<Columns, Rows, Type>::operator-=(const basic_matrix<Columns, Rows, Other>& other) noexcept -> basic_matrix& {
  for (auto i : std::views::iota(0u, Columns)) {
    _columns[i] -= other[i];
  }

  return *this;
}

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
template<scalar Other>
inline constexpr auto basic_matrix<Columns, Rows, Type>::operator*=(Other scalar) noexcept -> basic_matrix& {
  for (auto i : std::views::iota(0u, Columns)) {
    _columns[i] *= scalar;
  }

  return *this;
}

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
template<scalar Other>
inline constexpr auto basic_matrix<Columns, Rows, Type>::operator/=(Other scalar) noexcept -> basic_matrix& {
  for (auto i : std::views::iota(0u, Columns)) {
    _columns[i] /= scalar;
  }

  return *this;
}

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
inline constexpr auto basic_matrix<Columns, Rows, Type>::row(const size_type row) const noexcept -> basic_vector<Columns, value_type> {
  auto vector = basic_vector<Columns, value_type>{};

  for (auto i : std::views::iota(0u, Columns)) {
    vector[i] = _columns[i][row];
  }

  return vector;	
}

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
template<typename... Args>
inline constexpr basic_matrix<Columns, Rows, Type>::basic_matrix(Args&&... args) noexcept
: _columns{utility::make_array<column_type, Columns>(std::forward<Args>(args)...)} { }

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
inline constexpr auto basic_matrix<Columns, Rows, Type>::identity(const value_type value) noexcept -> basic_matrix {
  auto matrix = basic_matrix{};

  for (auto i : std::views::iota(0u, Columns)) {
    matrix[i][i] = value;
  }

  return matrix;
}

template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
inline constexpr auto operator==(const basic_matrix<Columns, Rows, Lhs>& lhs, const basic_matrix<Columns, Rows, Rhs>& rhs) noexcept -> bool {
  for (auto i : std::views::iota(0u, Columns)) {
    if (lhs[i] != rhs[i]) {
      return false;
    }
  }

  return true;
}

template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
inline constexpr auto operator+(basic_matrix<Columns, Rows, Lhs> lhs, const basic_matrix<Columns, Rows, Rhs>& rhs) noexcept -> basic_matrix<Columns, Rows, Lhs> {
  return lhs += rhs;
}

template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
inline constexpr auto operator-(basic_matrix<Columns, Rows, Lhs> lhs, const basic_matrix<Columns, Rows, Rhs>& rhs) noexcept -> basic_matrix<Columns, Rows, Lhs> {
  return lhs -= rhs;
}

template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
inline constexpr auto operator*(basic_matrix<Columns, Rows, Lhs> lhs, Rhs rhs) noexcept -> basic_matrix<Columns, Rows, Lhs> {
  return lhs *= rhs;
}

template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
inline constexpr auto operator*(Lhs lhs, basic_matrix<Columns, Rows, Rhs> rhs) noexcept -> basic_matrix<Columns, Rows, Rhs> {
  return rhs *= lhs;
}

template<typename Matrix>
inline constexpr auto from_array(std::span<typename Matrix::value_type, Matrix::columns * Matrix::rows> array) -> Matrix {
  return Matrix{*reinterpret_cast<Matrix*>(array.data())};
}

} // namespace sbx::math
