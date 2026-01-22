// SPDX-License-Identifier: MIT
/**
 * @file libsbx/math/matrix.hpp
 * 
 * @brief Generic fixed-size matrix type.
 *
 * @details
 * 
 * This header defines a statically sized, column-major matrix template
 * parameterized by column count, row count, and scalar type.
 *
 * The matrix is implemented as an array of column vectors and supports
 * element access, arithmetic operations, row extraction, and identity
 * construction. It is intended as a low-level building block for higher-
 * level math types such as transforms and projections.
 * 
 * @author KAJ
 *
 * @copyright (C) 2026 Jonas Kabelitz
 *
 * @package libsbx::math
 * @version 0.1.0
 * @since 2024-01-08
 */

#ifndef LIBSBX_MATH_MATRIX_HPP_
#define LIBSBX_MATH_MATRIX_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/vector.hpp>

namespace sbx::math {

/**
 * @brief Fixed-size column-major matrix.
 *
 * @tparam Columns Number of columns.
 * @tparam Rows    Number of rows.
 * @tparam Type    Scalar value type.
 */
template<std::size_t Columns, std::size_t Rows, scalar Type>
requires (Columns > 1u && Rows > 1u)
class basic_matrix {

public:

  /**
   * @brief Matrix traversal direction.
   */
  enum class direction : std::uint8_t {
    column = 0u,
    row = 1u
  }; // enum class direction

  /**
   * @brief Number of matrix columns.
   */
  inline static constexpr auto columns = Columns;

  /**
   * @brief Number of matrix rows.
   */
  inline static constexpr auto rows = Rows;

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using size_type = std::size_t;
  using column_type = basic_vector<Rows, Type>;

  /**
   * @brief Constructs a zero-initialized matrix.
   */
  constexpr basic_matrix() noexcept;

  /**
   * @brief Constructs a matrix with all elements set to a value.
   *
   * @tparam Other Scalar type.
   *
   * @param value Initialization value.
   */
  template<scalar Other = value_type>
  constexpr basic_matrix(const Other value) noexcept;

  /**
   * @brief Constructs a matrix from another matrix with convertible type.
   *
   * @tparam Other Source scalar type.
   *
   * @param other Source matrix.
   */
  template<scalar Other = value_type>
  constexpr basic_matrix(const basic_matrix<Columns, Rows, Other>& other) noexcept;

  constexpr basic_matrix(const basic_matrix& other) noexcept = default;

  constexpr basic_matrix(basic_matrix&& other) noexcept = default;

  auto operator=(const basic_matrix& other) noexcept -> basic_matrix& = default;

  auto operator=(basic_matrix&& other) noexcept -> basic_matrix& = default;

  /**
   * @brief Accesses a column by index.
   *
   * @param index Column index.
   *
   * @return Reference to the column.
   */
  [[nodiscard]] constexpr auto operator[](size_type index) noexcept -> column_type&;

  /**
   * @brief Accesses a column by index (const).
   *
   * @param index Column index.
   *
   * @return Const reference to the column.
   */
  [[nodiscard]] constexpr auto operator[](size_type index) const noexcept -> const column_type&;

  /**
   * @brief Adds another matrix to this matrix.
   *
   * @tparam Other Scalar type of the operand.
   *
   * @param other Right-hand side matrix.
   *
   * @return Reference to this matrix.
   */
  template<scalar Other>
  constexpr auto operator+=(const basic_matrix<Columns, Rows, Other>& other) noexcept -> basic_matrix&;

  /**
   * @brief Subtracts another matrix from this matrix.
   *
   * @tparam Other Scalar type of the operand.
   *
   * @param other Right-hand side matrix.
   *
   * @return Reference to this matrix.
   */
  template<scalar Other>
  constexpr auto operator-=(const basic_matrix<Columns, Rows, Other>& other) noexcept -> basic_matrix&;

  /**
   * @brief Multiplies this matrix by a scalar.
   *
   * @tparam Other Scalar type.
   *
   * @param scalar Scalar multiplier.
   *
   * @return Reference to this matrix.
   */
  template<scalar Other>
  constexpr auto operator*=(Other scalar) noexcept -> basic_matrix&;

  /**
   * @brief Divides this matrix by a scalar.
   *
   * @tparam Other Scalar type.
   *
   * @param scalar Scalar divisor.
   *
   * @return Reference to this matrix.
   */
  template<scalar Other>
  constexpr auto operator/=(Other scalar) noexcept -> basic_matrix&;

  /**
   * @brief Extracts a row as a vector.
   *
   * @param row Row index.
   *
   * @return Row vector.
   */
  constexpr auto row(const size_type row) const noexcept -> basic_vector<Columns, value_type>;

  /**
   * @brief Returns a pointer to the underlying contiguous storage.
   *
   * @return Pointer to the first scalar element.
   */
  constexpr auto data() noexcept -> value_type*;

  /**
   * @brief Returns a pointer to the underlying contiguous storage (const).
   *
   * @return Pointer to the first scalar element.
   */
  constexpr auto data() const noexcept -> const value_type*;

protected:

  template<scalar Other>
  using column_type_for = basic_vector<Rows, Other>;

  /**
   * @brief Constructs a matrix from a parameter pack of scalar values.
   *
   * @tparam Args Scalar argument types.
   *
   * @param args Scalar values used to initialize internal columns.
   */
  template<typename... Args>
  constexpr basic_matrix(Args&&... args) noexcept;

  /**
   * @brief Constructs an identity matrix.
   *
   * @param value Diagonal value.
   *
   * @return Identity matrix.
   */
  constexpr static auto identity(const value_type value = static_cast<value_type>(1)) noexcept -> basic_matrix;

private:

  std::array<column_type, Columns> _columns;

}; // class basic_matrix

/**
 * @brief Equality comparison for matrices.
 *
 * @tparam Columns Column count.
 * @tparam Rows    Row count.
 * @tparam Lhs     Left-hand scalar type.
 * @tparam Rhs     Right-hand scalar type.
 *
 * @param lhs Left-hand side matrix.
 * @param rhs Right-hand side matrix.
 *
 * @return True if all elements are equal.
 */
template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator==(const basic_matrix<Columns, Rows, Lhs>& lhs, const basic_matrix<Columns, Rows, Rhs>& rhs) noexcept -> bool;

/**
 * @brief Matrix addition.
 *
 * @tparam Columns Column count.
 * @tparam Rows    Row count.
 * @tparam Lhs     Left-hand scalar type.
 * @tparam Rhs     Right-hand scalar type.
 *
 * @param lhs Left-hand side matrix.
 * @param rhs Right-hand side matrix.
 *
 * @return Sum of both matrices.
 */
template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator+(basic_matrix<Columns, Rows, Lhs> lhs, const basic_matrix<Columns, Rows, Rhs>& rhs) noexcept -> basic_matrix<Columns, Rows, Lhs>;

/**
 * @brief Matrix subtraction.
 *
 * @tparam Columns Column count.
 * @tparam Rows    Row count.
 * @tparam Lhs     Left-hand scalar type.
 * @tparam Rhs     Right-hand scalar type.
 *
 * @param lhs Left-hand side matrix.
 * @param rhs Right-hand side matrix.
 *
 * @return Difference of both matrices.
 */
template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator-(basic_matrix<Columns, Rows, Lhs> lhs, const basic_matrix<Columns, Rows, Rhs>& rhs) noexcept -> basic_matrix<Columns, Rows, Lhs>;

/**
 * @brief Scalar multiplication.
 *
 * @tparam Columns Column count.
 * @tparam Rows    Row count.
 * @tparam Lhs     Left-hand scalar type.
 * @tparam Rhs     Right-hand scalar type.
 *
 * @param lhs Left-hand side matrix.
 * @param rhs Scalar multiplier.
 *
 * @return Scaled matrix.
 */
template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_matrix<Columns, Rows, Lhs> lhs, Rhs rhs) noexcept -> basic_matrix<Columns, Rows, Lhs>;

/**
 * @brief Scalar multiplication (reversed).
 *
 * @tparam Columns Column count.
 * @tparam Rows    Row count.
 * @tparam Lhs     Left-hand scalar type.
 * @tparam Rhs     Right-hand scalar type.
 *
 * @param lhs Scalar multiplier.
 * @param rhs Right-hand side matrix.
 *
 * @return Scaled matrix.
 */
template<std::size_t Columns, std::size_t Rows, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(Lhs lhs, basic_matrix<Columns, Rows, Rhs> rhs) noexcept -> basic_matrix<Columns, Rows, Rhs>;

/**
 * @brief Constructs a matrix from a contiguous array.
 *
 * @tparam Matrix Target matrix type.
 *
 * @param array Source array containing matrix elements.
 *
 * @return Constructed matrix.
 */
template<typename Matrix>
[[nodiscard]] constexpr auto from_array(std::span<typename Matrix::value_type, Matrix::columns * Matrix::rows> array) -> Matrix;

} // namespace sbx::math

#include <libsbx/math/matrix.ipp>

#endif // LIBSBX_MATH_MATRIX_HPP_
