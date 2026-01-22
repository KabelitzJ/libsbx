// SPDX-License-Identifier: MIT
/**
 * @file libsbx/math/vector.hpp
 * 
 * @brief Generic fixed-size vector type.
 *
 * @details
 * 
 * This header defines a statically sized vector template parameterized by
 * component count and scalar type.
 *
 * The vector stores its components contiguously and supports basic arithmetic,
 * interpolation, normalization, and utility operations such as component-wise
 * min/max and absolute value.
 * 
 * @author KAJ
 *
 * @copyright (C) 2026 Jonas Kabelitz
 *
 * @package libsbx::math
 * @version 0.1.0
 * @since 2024-01-06
 */

#ifndef LIBSBX_MATH_VECTOR_HPP_
#define LIBSBX_MATH_VECTOR_HPP_

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <ranges>
#include <utility>

#include <libsbx/utility/make_array.hpp>
#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/zip.hpp>
#include <libsbx/utility/hash.hpp>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/traits.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/algorithm.hpp>

namespace sbx::math {

/**
 * @brief Fixed-size vector type.
 *
 * @tparam Size Component count.
 * @tparam Type Scalar component type.
 */
template<std::size_t Size, scalar Type>
requires (Size > 1u)
class basic_vector {

  template<std::size_t S, scalar T>
  requires (S > 1u)
  friend class basic_vector;

public:

  using value_type = Type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using size_type = std::size_t;
  using length_type = std::float_t;

  /**
   * @brief Constructs a vector with all components initialized to a value.
   *
   * @tparam Other Scalar type.
   *
   * @param value Initialization value.
   */
  template<scalar Other = value_type>
  constexpr basic_vector(Other value = Other{0}) noexcept;

  /**
   * @brief Constructs a vector from another vector with convertible scalar type.
   *
   * @tparam Other Source scalar type.
   *
   * @param other Source vector.
   */
  template<scalar Other = value_type>
  constexpr basic_vector(const basic_vector<Size, Other>& other) noexcept;

  constexpr basic_vector(const basic_vector& other) noexcept = default;
  constexpr basic_vector(basic_vector&& other) noexcept = default;

  auto operator=(const basic_vector& other) noexcept -> basic_vector& = default;
  auto operator=(basic_vector&& other) noexcept -> basic_vector& = default;

  /**
   * @brief Returns the minimum component of a vector.
   *
   * @param vector Input vector.
   *
   * @return Smallest component value.
   */
  static constexpr auto min(const basic_vector& vector) noexcept -> value_type;

  /**
   * @brief Returns the component-wise minimum of two vectors.
   *
   * @tparam Lhs Left-hand scalar type.
   * @tparam Rhs Right-hand scalar type.
   *
   * @param lhs Left-hand side vector.
   * @param rhs Right-hand side vector.
   *
   * @return Component-wise minimum vector.
   */
  template<scalar Lhs = value_type, scalar Rhs = value_type>
  static constexpr auto min(const basic_vector<Size, Lhs>& lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> basic_vector;

  /**
   * @brief Returns the maximum component of a vector.
   *
   * @param vector Input vector.
   *
   * @return Largest component value.
   */
  static constexpr auto max(const basic_vector& vector) noexcept -> value_type;

  /**
   * @brief Returns the component-wise maximum of two vectors.
   *
   * @tparam Lhs Left-hand scalar type.
   * @tparam Rhs Right-hand scalar type.
   *
   * @param lhs Left-hand side vector.
   * @param rhs Right-hand side vector.
   *
   * @return Component-wise maximum vector.
   */
  template<scalar Lhs = value_type, scalar Rhs = value_type>
  static constexpr auto max(const basic_vector<Size, Lhs>& lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> basic_vector;

  /**
   * @brief Returns the component-wise absolute value of a vector.
   *
   * @tparam Lhs Scalar type of the input vector.
   *
   * @param vector Input vector.
   *
   * @return Vector with absolute component values.
   */
  template<scalar Lhs = value_type>
  static constexpr auto abs(const basic_vector<Size, Lhs>& vector) noexcept -> basic_vector;

  /**
   * @brief Splat a single component across all axes.
   *
   * @tparam Axis  Component index to splat.
   * @tparam Other Scalar type.
   *
   * @param vector Input vector.
   *
   * @return Vector with all components set to vector[Axis].
   */
  template<size_type Axis, scalar Other = value_type>
  requires (Axis < Size)
  [[nodiscard]] static constexpr auto splat(const basic_vector<Size, Other>& vector) noexcept -> basic_vector<Size, Other>;

  /**
   * @brief Linearly interpolates between two vectors.
   *
   * @param x Start vector.
   * @param y End vector.
   * @param a Interpolation factor.
   *
   * @return Interpolated vector.
   */
  [[nodiscard]] static constexpr auto lerp(const basic_vector& x, const basic_vector& y, const value_type a) noexcept -> basic_vector;

  /**
   * @brief Returns a pointer to the underlying contiguous storage.
   *
   * @return Pointer to the first component.
   */
  constexpr auto data() noexcept -> value_type*;

  /**
   * @brief Returns a pointer to the underlying contiguous storage (const).
   *
   * @return Pointer to the first component.
   */
  constexpr auto data() const noexcept -> const value_type*;

  /**
   * @brief Accesses a component by index.
   *
   * @param index Component index.
   *
   * @return Reference to the component.
   */
  [[nodiscard]] constexpr auto operator[](size_type index) noexcept -> reference;

  /**
   * @brief Accesses a component by index (const).
   *
   * @param index Component index.
   *
   * @return Const reference to the component.
   */
  [[nodiscard]] constexpr auto operator[](size_type index) const noexcept -> const_reference;

  /**
   * @brief Adds another vector to this vector.
   *
   * @tparam Other Scalar type of the operand.
   *
   * @param other Right-hand side vector.
   *
   * @return Reference to this vector.
   */
  template<scalar Other>
  constexpr auto operator+=(const basic_vector<Size, Other>& other) noexcept -> basic_vector&;

  /**
   * @brief Subtracts another vector from this vector.
   *
   * @tparam Other Scalar type of the operand.
   *
   * @param other Right-hand side vector.
   *
   * @return Reference to this vector.
   */
  template<scalar Other>
  constexpr auto operator-=(const basic_vector<Size, Other>& other) noexcept -> basic_vector&;

  /**
   * @brief Multiplies this vector by a scalar.
   *
   * @tparam Other Scalar type.
   *
   * @param scalar Scalar multiplier.
   *
   * @return Reference to this vector.
   */
  template<scalar Other>
  constexpr auto operator*=(Other scalar) noexcept -> basic_vector&;

  /**
   * @brief Multiplies this vector component-wise by another vector.
   *
   * @tparam Other Scalar type of the operand.
   *
   * @param other Right-hand side vector.
   *
   * @return Reference to this vector.
   */
  template<scalar Other>
  constexpr auto operator*=(const basic_vector<Size, Other>& other) noexcept -> basic_vector&;

  /**
   * @brief Divides this vector by a scalar.
   *
   * @tparam Other Scalar type.
   *
   * @param scalar Scalar divisor.
   *
   * @return Reference to this vector.
   */
  template<scalar Other>
  constexpr auto operator/=(Other scalar) noexcept -> basic_vector&;

  /**
   * @brief Returns the squared length of the vector.
   *
   * @return Squared vector length.
   */
  [[nodiscard]] constexpr auto length_squared() const noexcept -> length_type;

  /**
   * @brief Returns the length of the vector.
   *
   * @return Vector length.
   */
  [[nodiscard]] constexpr auto length() const noexcept -> length_type;

  /**
   * @brief Normalizes the vector in-place.
   *
   * @return Reference to this vector.
   */
  constexpr auto normalize() noexcept -> basic_vector&;

protected:

  template<std::convertible_to<value_type>... Args>
  requires (sizeof...(Args) == Size)
  constexpr basic_vector(Args&&... args) noexcept;

  constexpr basic_vector(std::array<value_type, Size>&& components) noexcept;

  template<scalar Other>
  [[nodiscard]] static constexpr auto fill(Other value) noexcept -> basic_vector;

  template<std::size_t Index, scalar Other>
  [[nodiscard]] static constexpr auto axis(Other value) noexcept -> basic_vector;

private:

  std::array<Type, Size> _components;

}; // class basic_vector

/**
 * @brief Equality comparison for vectors.
 *
 * @tparam Size Component count.
 * @tparam Lhs  Left-hand scalar type.
 * @tparam Rhs  Right-hand scalar type.
 *
 * @param lhs Left-hand side vector.
 * @param rhs Right-hand side vector.
 *
 * @return True if all components are equal.
 */
template<std::size_t Size, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator==(const basic_vector<Size, Lhs>& lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> bool;

/**
 * @brief Vector addition.
 *
 * @tparam Size Component count.
 * @tparam Lhs  Left-hand scalar type.
 * @tparam Rhs  Right-hand scalar type.
 *
 * @param lhs Left-hand side vector.
 * @param rhs Right-hand side vector.
 *
 * @return Sum of both vectors.
 */
template<std::size_t Size, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator+(basic_vector<Size, Lhs> lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> basic_vector<Size, Lhs>;

/**
 * @brief Vector subtraction.
 *
 * @tparam Size Component count.
 * @tparam Lhs  Left-hand scalar type.
 * @tparam Rhs  Right-hand scalar type.
 *
 * @param lhs Left-hand side vector.
 * @param rhs Right-hand side vector.
 *
 * @return Difference of both vectors.
 */
template<std::size_t Size, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator-(basic_vector<Size, Lhs> lhs, const basic_vector<Size, Rhs>& rhs) noexcept -> basic_vector<Size, Lhs>;

/**
 * @brief Scalar multiplication.
 *
 * @tparam Size Component count.
 * @tparam Lhs  Left-hand scalar type.
 * @tparam Rhs  Right-hand scalar type.
 *
 * @param lhs Left-hand side vector.
 * @param scalar Scalar multiplier.
 *
 * @return Scaled vector.
 */
template<std::size_t Size, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator*(basic_vector<Size, Lhs> lhs, Rhs scalar) noexcept -> basic_vector<Size, Lhs>;

/**
 * @brief Scalar division.
 *
 * @tparam Size Component count.
 * @tparam Lhs  Left-hand scalar type.
 * @tparam Rhs  Right-hand scalar type.
 *
 * @param lhs Left-hand side vector.
 * @param scalar Scalar divisor.
 *
 * @return Scaled vector.
 */
template<std::size_t Size, scalar Lhs, scalar Rhs>
[[nodiscard]] constexpr auto operator/(basic_vector<Size, Lhs> lhs, Rhs scalar) noexcept -> basic_vector<Size, Lhs>;

} // namespace sbx::math

#include <libsbx/math/vector.ipp>

#endif // LIBSBX_MATH_VECTOR_HPP_
