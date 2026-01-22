// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_MATRIX_HPP_
#define LIBSBX_MATH_MATRIX_HPP_

#include <array>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/vector.hpp>

namespace sbx::math {

template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
class basic_matrix {

public:

  enum class direction : std::uint8_t {
    column = 0u,
    row = 1u
  }; // enum class direction

  inline static constexpr auto columns = Columns;
  inline static constexpr auto rows = Rows;

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using size_type = std::size_t;
  using column_type = basic_vector<Rows, Type>;

  constexpr basic_matrix() noexcept;

  template<scalar Other = value_type>
  constexpr basic_matrix(const Other value) noexcept;

  template<scalar Other = value_type>
  constexpr basic_matrix(const basic_matrix<Columns, Rows, Other>& other) noexcept;

  constexpr basic_matrix(const basic_matrix& other) noexcept = default;

  constexpr basic_matrix(basic_matrix&& other) noexcept = default;

  auto operator=(const basic_matrix& other) noexcept -> basic_matrix& = default;

  auto operator=(basic_matrix&& other) noexcept -> basic_matrix& = default;

  [[nodiscard]] constexpr auto operator[](size_type index) noexcept -> column_type&;

  [[nodiscard]] constexpr auto operator[](size_type index) const noexcept -> const column_type&;

  template<scalar Other>
  constexpr auto operator+=(const basic_matrix<Columns, Rows, Other>& other) noexcept -> basic_matrix&;

  template<scalar Other>
  constexpr auto operator-=(const basic_matrix<Columns, Rows, Other>& other) noexcept -> basic_matrix&;

  template<scalar Other>
  constexpr auto operator*=(Other scalar) noexcept -> basic_matrix&;

  template<scalar Other>
  constexpr auto operator/=(Other scalar) noexcept -> basic_matrix&;

  constexpr auto row(const size_type row) const noexcept -> basic_vector<Columns, value_type>;

  constexpr auto data() noexcept -> value_type* {
    return _columns[0].data();
  }

  constexpr auto data() const noexcept -> const value_type* {
    return _columns[0].data();
  }

protected:

  template<scalar Other>
  using column_type_for = basic_vector<Rows, Other>;

  template<typename... Args>
  constexpr basic_matrix(Args&&... args) noexcept;

  constexpr static auto identity(const value_type value = static_cast<value_type>(1)) noexcept -> basic_matrix;

private:

  std::array<column_type, Columns> _columns;

}; // class basic_matrix

template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator==(const basic_matrix<Columns, Rows, Lhs>& lhs, const basic_matrix<Columns, Rows, Rhs>& rhs) noexcept -> bool;

template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator+(basic_matrix<Columns, Rows, Lhs> lhs, const basic_matrix<Columns, Rows, Rhs>& rhs) noexcept -> basic_matrix<Columns, Rows, Lhs>;

template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator-(basic_matrix<Columns, Rows, Lhs> lhs, const basic_matrix<Columns, Rows, Rhs>& rhs) noexcept -> basic_matrix<Columns, Rows, Lhs>;

template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_matrix<Columns, Rows, Lhs> lhs, Rhs rhs) noexcept -> basic_matrix<Columns, Rows, Lhs>;

template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(Lhs lhs, basic_matrix<Columns, Rows, Rhs> rhs) noexcept -> basic_matrix<Columns, Rows, Rhs>;

template<typename Matrix>
[[nodiscard]] constexpr auto from_array(std::span<typename Matrix::value_type, Matrix::columns * Matrix::rows> array) -> Matrix;

} // namespace sbx::math

#include <libsbx/math/matrix.ipp>

#endif // LIBSBX_MATH_MATRIX_HPP_
