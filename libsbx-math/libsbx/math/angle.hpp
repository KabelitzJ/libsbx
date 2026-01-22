// SPDX-License-Identifier: MIT
/**
 * @file libsbx/math/angle.hpp
 * 
 * @brief Angle types and utilities.
 *
 * @details
 * 
 * This header defines strongly-typed wrappers for angular quantities in degrees and radians,
 * plus a unified angle type that stores angles internally in radians.
 *
 * The provided types are lightweight value wrappers intended to prevent unit-mismatch bugs,
 * while remaining usable in constexpr contexts where possible.
 * 
 * @author KAJ
 *
 * @copyright (C) 2026 Jonas Kabelitz
 *
 * @package libsbx::math
 * @version 0.1.0
 * @since 2023-05-24
 */

#ifndef LIBSBX_MATH_ANGLE_HPP_
#define LIBSBX_MATH_ANGLE_HPP_

#include <cmath>
#include <concepts>
#include <numbers>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/smooth_value.hpp>

namespace sbx::math {

/**
 * @brief Strongly-typed degree value wrapper.
 *
 * This type represents an angle measured in degrees. It is a thin wrapper around a floating point
 * storage type, enabling explicit unit usage and overload selection.
 *
 * @tparam Type Floating-point storage type.
 */
template<floating_point Type>
class basic_degree {

public:

  /**
   * @brief Underlying scalar value type.
   */
  using value_type = Type;

  /**
   * @brief Minimum representable canonical degree value used by this library.
   *
   * This value is intended for clamping and range conventions used by the library.
   */
  inline static constexpr basic_degree min{value_type{0}};

  /**
   * @brief Maximum representable canonical degree value used by this library.
   *
   * This value is intended for clamping and range conventions used by the library.
   */
  inline static constexpr basic_degree max{value_type{360}};

  /**
   * @brief Constructs a degree value initialized to zero.
   */
  constexpr basic_degree() = default;

  /**
   * @brief Constructs a degree value from a scalar.
   *
   * @tparam Other Scalar input type convertible to @ref value_type.
   *
   * @param value Degree value to store.
   */
  template<scalar Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr basic_degree(const Other value) noexcept;

  /**
   * @brief Constructs a degree value from another degree type.
   *
   * @tparam Other Floating-point storage type of the source.
   *
   * @param other Source degree value.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr basic_degree(const basic_degree<Other>& other) noexcept;

  /**
   * @brief Destructor.
   */
  constexpr ~basic_degree() noexcept = default;

  /**
   * @brief Assigns from another degree type.
   *
   * @tparam Other Floating-point storage type of the source.
   *
   * @param other Source degree value.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator=(const basic_degree<Other>& other) noexcept -> basic_degree<Type>&;

  /**
   * @brief Adds another degree value to this value.
   *
   * @tparam Other Floating-point storage type of the operand.
   *
   * @param rhs Right-hand side operand.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator+=(const basic_degree<Other>& rhs) noexcept -> basic_degree<Type>&;

  /**
   * @brief Subtracts another degree value from this value.
   *
   * @tparam Other Floating-point storage type of the operand.
   *
   * @param rhs Right-hand side operand.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator-=(const basic_degree<Other>& rhs) noexcept -> basic_degree<Type>&;

  /**
   * @brief Scales this degree value by a floating point factor.
   *
   * @tparam Other Floating-point type convertible to @ref value_type.
   *
   * @param rhs Scale factor.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator*=(const Other rhs) noexcept -> basic_degree<Type>&;

  /**
   * @brief Divides this degree value by a floating point factor.
   *
   * @tparam Other Floating-point type convertible to @ref value_type.
   *
   * @param rhs Divisor.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator/=(const Other rhs) noexcept -> basic_degree<Type>&;

  /**
   * @brief Returns the stored degree value.
   *
   * @return Stored degree value.
   */
  constexpr auto value() const noexcept -> value_type;

  /**
   * @brief Implicit conversion to the underlying scalar value.
   *
   * @return Stored degree value.
   */
  constexpr operator value_type() const noexcept;

  /**
   * @brief Returns a pointer to the underlying stored value.
   *
   * @return Pointer to the stored degree value.
   */
  constexpr auto data() noexcept -> value_type*;

private:

  /**
   * @brief Stored degree value.
   */
  value_type _value{};

}; // class basic_degree

/**
 * @brief Equality comparison for degrees.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return True if the underlying values compare equal.
 */
template<floating_point Type>
constexpr auto operator==(const basic_degree<Type>& lhs, const basic_degree<Type>& rhs) noexcept -> bool;

/**
 * @brief Partial ordering comparison for degrees.
 *
 * @tparam Type Floating-point storage type of the left operand.
 * @tparam Other Floating-point storage type of the right operand.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return Partial ordering result of the underlying values.
 */
template<floating_point Type, floating_point Other>
constexpr auto operator<=>(const basic_degree<Type>& lhs, const basic_degree<Other>& rhs) noexcept -> std::partial_ordering;

/**
 * @brief Adds two degree values.
 *
 * @tparam Type Floating-point storage type of the left operand.
 * @tparam Other Floating-point storage type of the right operand.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return Sum of both operands.
 */
template<floating_point Type, floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto operator+(basic_degree<Type> lhs, const basic_degree<Other>& rhs) noexcept -> basic_degree<Type>;

/**
 * @brief Subtracts two degree values.
 *
 * @tparam Type Floating-point storage type of the left operand.
 * @tparam Other Floating-point storage type of the right operand.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return Difference of both operands.
 */
template<floating_point Type, floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto operator-(basic_degree<Type> lhs, const basic_degree<Other>& rhs) noexcept -> basic_degree<Type>;

/**
 * @brief Multiplies a degree value by a scalar factor.
 *
 * @tparam Type Floating-point storage type.
 * @tparam Other Scalar type convertible to @ref Type.
 *
 * @param lhs Degree value.
 * @param rhs Scale factor.
 *
 * @return Scaled degree value.
 */
template<floating_point Type, std::convertible_to<Type> Other>
constexpr auto operator*(basic_degree<Type> lhs, const Other rhs) noexcept -> basic_degree<Type>;

/**
 * @brief Divides a degree value by a scalar factor.
 *
 * @tparam Type Floating-point storage type.
 * @tparam Other Scalar type convertible to @ref Type.
 *
 * @param lhs Degree value.
 * @param rhs Divisor.
 *
 * @return Scaled degree value.
 */
template<floating_point Type, std::convertible_to<Type> Other>
constexpr auto operator/(basic_degree<Type> lhs, const Other rhs) noexcept -> basic_degree<Type>;

/**
 * @brief Clamps a degree value to a closed interval.
 *
 * @tparam Type Floating-point storage type.
 * @param value Value to clamp.
 *
 * @param min Lower bound (inclusive).
 * @param max Upper bound (inclusive).
 *
 * @return Reference to the clamped value (one of value, min, or max).
 */
template<floating_point Type>
constexpr auto clamp(const basic_degree<Type>& value, const basic_degree<Type>& min, const basic_degree<Type>& max) -> const basic_degree<Type>&;

/**
 * @brief Default degree type using @ref std::float_t.
 */
using degree = basic_degree<std::float_t>;

/**
 * @brief Marks degrees as smoothable for smoothing/interpolation utilities.
 *
 * This integrates with the library's smooth-value infrastructure.
 */
template<floating_point Type>
struct is_smoothable<basic_degree<Type>> : std::true_type { };

/**
 * @brief Comparison traits specialization for degree values.
 *
 * Delegates comparison semantics to the comparison traits of the underlying scalar type.
 *
 * @tparam Type Floating-point storage type.
 */
template<floating_point Type>
struct comparision_traits<basic_degree<Type>> {

  using base_trait = comparision_traits<Type>;

  /**
   * @brief Compares two degree values for equality using the base trait.
   *
   * @param lhs Left-hand side operand.
   * @param rhs Right-hand side operand.
   *
   * @return True if the underlying values are considered equal by the base trait.
   */
  inline static constexpr auto equal(const basic_degree<Type>& lhs, const basic_degree<Type>& rhs) noexcept -> bool;

}; // struct comparision_traits

/**
 * @brief Linearly interpolates between two degree values.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param x Start value.
 * @param y End value.
 * @param a Interpolation factor in [0, 1] (not enforced).
 *
 * @return Interpolated degree value.
 */
template<floating_point Type>
inline constexpr auto mix(const basic_degree<Type> x, const basic_degree<Type> y, const Type a) -> basic_degree<Type>;

/**
 * @brief Strongly-typed radian value wrapper.
 *
 * This type represents an angle measured in radians. It is a thin wrapper around a floating point
 * storage type, enabling explicit unit usage and overload selection.
 *
 * @tparam Type Floating-point storage type.
 */
template<floating_point Type>
class basic_radian {

public:

  /**
   * @brief Underlying scalar value type.
   */
  using value_type = Type;

  /**
   * @brief Minimum representable canonical radian value used by this library.
   *
   * This value is intended for clamping and range conventions used by the library.
   */
  inline static constexpr basic_radian min{value_type{0}};

  /**
   * @brief Maximum representable canonical radian value used by this library.
   *
   * This value is intended for clamping and range conventions used by the library.
   */
  inline static constexpr basic_radian max{value_type{2 * std::numbers::pi_v<value_type>}};

  /**
   * @brief Constructs a radian value initialized to zero.
   */
  constexpr basic_radian() = default;

  /**
   * @brief Constructs a radian value from a scalar.
   *
   * @tparam Other Scalar input type convertible to @ref value_type.
   *
   * @param value Radian value to store.
   */
  template<scalar Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr basic_radian(const Other value) noexcept;

  /**
   * @brief Constructs a radian value from another radian type.
   *
   * @tparam Other Floating-point storage type of the source.
   *
   * @param other Source radian value.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr basic_radian(const basic_radian<Other>& other) noexcept;

  /**
   * @brief Destructor.
   */
  constexpr ~basic_radian() noexcept = default;

  /**
   * @brief Assigns from another radian type.
   *
   * @tparam Other Floating-point storage type of the source.
   *
   * @param other Source radian value.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator=(const basic_radian<Other>& other) noexcept -> basic_radian<Type>&;

  /**
   * @brief Adds another radian value to this value.
   *
   * @tparam Other Floating-point storage type of the operand.
   *
   * @param rhs Right-hand side operand.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator+=(const basic_radian<Other>& rhs) noexcept -> basic_radian<Type>&;

  /**
   * @brief Subtracts another radian value from this value.
   *
   * @tparam Other Floating-point storage type of the operand.
   *
   * @param rhs Right-hand side operand.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator-=(const basic_radian<Other>& rhs) noexcept -> basic_radian<Type>&;

  /**
   * @brief Scales this radian value by a floating point factor.
   *
   * @tparam Other Floating-point type convertible to @ref value_type.
   *
   * @param rhs Scale factor.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator*=(const Other rhs) noexcept -> basic_radian<Type>&;

  /**
   * @brief Returns the stored radian value.
   *
   * @return Stored radian value.
   */
  constexpr auto value() const noexcept -> value_type;

  /**
   * @brief Implicit conversion to the underlying scalar value.
   *
   * @return Stored radian value.
   */
  constexpr operator value_type() const noexcept;

  /**
   * @brief Returns a pointer to the underlying stored value.
   *
   * @return Pointer to the stored radian value.
   */
  constexpr auto data() noexcept -> value_type*;

private:

  /**
   * @brief Stored radian value.
   */
  value_type _value{};

}; // class basic_radian

/**
 * @brief Equality comparison for radians.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return True if the underlying values compare equal.
 */
template<floating_point Type>
constexpr auto operator==(const basic_radian<Type>& lhs, const basic_radian<Type>& rhs) noexcept -> bool;

/**
 * @brief Partial ordering comparison for radians.
 *
 * @tparam Type Floating-point storage type of the left operand.
 * @tparam Other Floating-point storage type of the right operand.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return Partial ordering result of the underlying values.
 */
template<floating_point Type, floating_point Other>
constexpr auto operator<=>(const basic_radian<Type>& lhs, const basic_radian<Other>& rhs) noexcept -> std::partial_ordering;

/**
 * @brief Adds two radian values.
 *
 * @tparam Type Floating-point storage type of the left operand.
 * @tparam Other Floating-point storage type of the right operand.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return Sum of both operands.
 */
template<floating_point Type, floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto operator+(basic_radian<Type> lhs, const basic_radian<Other>& rhs) noexcept -> basic_radian<Type>;

/**
 * @brief Subtracts two radian values.
 *
 * @tparam Type Floating-point storage type of the left operand.
 * @tparam Other Floating-point storage type of the right operand.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return Difference of both operands.
 */
template<floating_point Type, floating_point Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto operator-(basic_radian<Type> lhs, const basic_radian<Other>& rhs) noexcept -> basic_radian<Type>;

/**
 * @brief Multiplies a radian value by a scalar factor.
 *
 * @tparam Type Floating-point storage type.
 * @tparam Other Scalar type convertible to @ref Type.
 *
 * @param lhs Radian value.
 * @param rhs Scale factor.
 *
 * @return Scaled radian value.
 */
template<floating_point Type, std::convertible_to<Type> Other>
requires (std::is_convertible_v<Other, Type>)
constexpr auto operator*(basic_radian<Type> lhs, const Other rhs) noexcept -> basic_radian<Type>;

/**
 * @brief Clamps a radian value to a closed interval.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param value Value to clamp.
 * @param min Lower bound (inclusive).
 * @param max Upper bound (inclusive).
 *
 * @return Reference to the clamped value (one of value, min, or max).
 */
template<floating_point Type>
constexpr auto clamp(const basic_radian<Type>& value, const basic_radian<Type>& min, const basic_radian<Type>& max) -> const basic_radian<Type>&;

/**
 * @brief Default radian type using @ref std::float_t.
 */
using radian = basic_radian<std::float_t>;

/**
 * @brief Unified angle type stored internally in radians.
 *
 * This type provides explicit construction from degrees or radians, supports arithmetic, and provides
 * unit conversion helpers.
 *
 * @tparam Type Floating-point storage type.
 */
template<floating_point Type>
class basic_angle {

public:

  /**
   * @brief Underlying scalar value type.
   */
  using value_type = Type;

  /**
   * @brief Constructs an angle from a degree value.
   *
   * @param degree Angle expressed in degrees.
   */
  constexpr basic_angle(const basic_degree<value_type>& degree) noexcept;

  /**
   * @brief Constructs an angle from a radian value.
   *
   * @param radian Angle expressed in radians.
   */
  constexpr basic_angle(const basic_radian<value_type>& radian) noexcept;

  /**
   * @brief Constructs an angle from another angle type.
   *
   * @tparam Other Floating-point storage type of the source.
   *
   * @param other Source angle.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr basic_angle(const basic_angle<Other>& other) noexcept;

  /**
   * @brief Adds another angle to this angle.
   *
   * @tparam Other Floating-point storage type of the operand.
   *
   * @param other Angle to add.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator+=(const basic_angle<Other>& other) noexcept -> basic_angle<Type>&;

  /**
   * @brief Adds a degree value to this angle.
   *
   * @tparam Other Floating-point storage type of the operand.
   *
   * @param other Degree value to add.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator+=(const basic_degree<Other>& other) noexcept -> basic_angle<Type>&;

  /**
   * @brief Adds a radian value to this angle.
   *
   * @tparam Other Floating-point storage type of the operand.
   *
   * @param other Radian value to add.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator+=(const basic_radian<Other>& other) noexcept -> basic_angle<Type>&;

  /**
   * @brief Subtracts another angle from this angle.
   *
   * @tparam Other Floating-point storage type of the operand.
   *
   * @param other Angle to subtract.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator-=(const basic_angle<Other>& other) noexcept -> basic_angle<Type>&;

  /**
   * @brief Subtracts a degree value from this angle.
   *
   * @tparam Other Floating-point storage type of the operand.
   *
   * @param other Degree value to subtract.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator-=(const basic_degree<Other>& other) noexcept -> basic_angle<Type>&;

  /**
   * @brief Subtracts a radian value from this angle.
   *
   * @tparam Other Floating-point storage type of the operand.
   *
   * @param other Radian value to subtract.
   * @return Reference to this instance.
   *
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator-=(const basic_radian<Other>& other) noexcept -> basic_angle<Type>&;

  /**
   * @brief Scales this angle by a floating point factor.
   *
   * @tparam Other Floating-point type convertible to @ref value_type.
   *
   * @param other Scale factor.
   *
   * @return Reference to this instance.
   */
  template<floating_point Other>
  requires (std::is_convertible_v<Other, Type>)
  constexpr auto operator*=(const Other other) noexcept -> basic_angle<Type>&;

  /**
   * @brief Converts this angle to degrees.
   *
   * @return Angle expressed in degrees.
   */
  constexpr auto to_degrees() const noexcept -> basic_degree<value_type>;

  /**
   * @brief Converts this angle to radians.
   *
   * @return Angle expressed in radians.
   */
  constexpr auto to_radians() const noexcept -> basic_radian<value_type>;

private:

  /**
   * @brief Internal radian storage.
   */
  basic_radian<Type> _radian{};

}; // class basic_angle

/**
 * @brief Equality comparison for angles.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return True if the angles compare equal in radians.
 */
template<floating_point Type>
constexpr auto operator==(const basic_angle<Type>& lhs, const basic_angle<Type>& rhs) noexcept -> bool;

/**
 * @brief Partial ordering comparison for angles.
 *
 * @tparam Type Floating-point storage type of the left operand.
 * @tparam Other Floating-point storage type of the right operand.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return Partial ordering result of the radian values.
 */
template<floating_point Type, floating_point Other>
constexpr auto operator<=>(const basic_angle<Type>& lhs, const basic_angle<Other>& rhs) noexcept -> std::partial_ordering;

/**
 * @brief Adds two angles.
 *
 * @tparam LhsType Floating-point storage type of the left operand.
 * @tparam RhsType Floating-point storage type of the right operand.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return Sum of both operands.
 */
template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator+(basic_angle<LhsType> lhs, const basic_angle<RhsType>& rhs) noexcept -> basic_angle<LhsType>;

/**
 * @brief Adds an angle and a degree value.
 *
 * @tparam LhsType Floating-point storage type of the left operand.
 * @tparam RhsType Floating-point storage type of the degree operand.
 *
 * @param lhs Left-hand side angle.
 * @param rhs Degree value.
 *
 * @return Sum as an angle.
 */
template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator+(basic_angle<LhsType> lhs, const basic_degree<RhsType>& rhs) noexcept -> basic_angle<LhsType>;

/**
 * @brief Adds an angle and a radian value.
 *
 * @tparam LhsType Floating-point storage type of the left operand.
 * @tparam RhsType Floating-point storage type of the radian operand.
 *
 * @param lhs Left-hand side angle.
 * @param rhs Radian value.
 *
 * @return Sum as an angle.
 */
template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator+(basic_angle<LhsType> lhs, const basic_radian<RhsType>& rhs) noexcept -> basic_angle<LhsType>;

/**
 * @brief Subtracts two angles.
 *
 * @tparam LhsType Floating-point storage type of the left operand.
 * @tparam RhsType Floating-point storage type of the right operand.
 *
 * @param lhs Left-hand side operand.
 * @param rhs Right-hand side operand.
 *
 * @return Difference of both operands.
 */
template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator-(basic_angle<LhsType> lhs, const basic_angle<RhsType>& rhs) noexcept -> basic_angle<LhsType>;

/**
 * @brief Subtracts a degree value from an angle.
 *
 * @tparam LhsType Floating-point storage type of the left operand.
 * @tparam RhsType Floating-point storage type of the degree operand.
 *
 * @param lhs Left-hand side angle.
 * @param rhs Degree value.
 *
 * @return Difference as an angle.
 */
template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator-(basic_angle<LhsType> lhs, const basic_degree<RhsType>& rhs) noexcept -> basic_angle<LhsType>;

/**
 * @brief Subtracts a radian value from an angle.
 *
 * @tparam LhsType Floating-point storage type of the left operand.
 * @tparam RhsType Floating-point storage type of the radian operand.
 *
 * @param lhs Left-hand side angle.
 * @param rhs Radian value.
 *
 * @return Difference as an angle.
 */
template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator-(basic_angle<LhsType> lhs, const basic_radian<RhsType>& rhs) noexcept -> basic_angle<LhsType>;

/**
 * @brief Scales an angle by a scalar factor.
 *
 * @tparam LhsType Floating-point storage type of the angle.
 * @tparam RhsType Floating-point type of the scalar factor.
 *
 * @param lhs Angle value.
 * @param rhs Scale factor.
 *
 * @return Scaled angle.
 */
template<floating_point LhsType, floating_point RhsType>
requires (std::is_convertible_v<RhsType, LhsType>)
constexpr auto operator*(basic_angle<LhsType> lhs, const RhsType rhs) noexcept -> basic_angle<LhsType>;

/**
 * @brief Clamps an angle value to a closed interval.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param value Value to clamp.
 * @param min Lower bound (inclusive).
 * @param max Upper bound (inclusive).
 *
 * @return Reference to the clamped value (one of value, min, or max).
 */
template<floating_point Type>
constexpr auto clamp(const basic_angle<Type>& value, const basic_angle<Type>& min, const basic_angle<Type>& max) -> const basic_angle<Type>&;

/**
 * @brief Default angle type using std::float_t.
 */
using angle = basic_angle<std::float_t>;

/**
 * @brief Converts radians to degrees.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param radian Radian value to convert.
 *
 * @return Degree equivalent.
 */
template<floating_point Type>
constexpr auto to_degrees(const basic_radian<Type>& radian) noexcept -> basic_degree<Type>;

/**
 * @brief Converts degrees to radians.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param degree Degree value to convert.
 *
 * @return Radian equivalent.
 */
template<floating_point Type>
constexpr auto to_radians(const basic_degree<Type>& degree) noexcept -> basic_radian<Type>;

/**
 * @brief Computes the sine of an angle.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param angle Angle to evaluate.
 *
 * @return Sine of the angle.
 */
template<floating_point Type>
constexpr auto sin(const basic_angle<Type>& angle) noexcept -> Type;

/**
 * @brief Computes the sine of a degree value.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param degree Degree value to evaluate.
 *
 * @return Sine of the angle.
 */
template<floating_point Type>
constexpr auto sin(const basic_degree<Type>& degree) noexcept -> Type;

/**
 * @brief Computes the sine of a radian value.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param radian Radian value to evaluate.
 *
 * @return Sine of the angle.
 */
template<floating_point Type>
constexpr auto sin(const basic_radian<Type>& radian) noexcept -> Type;

/**
 * @brief Computes the cosine of an angle.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param angle Angle to evaluate.
 *
 * @return Cosine of the angle.
 */
template<floating_point Type>
constexpr auto cos(const basic_angle<Type>& angle) noexcept -> Type;

/**
 * @brief Computes the cosine of a degree value.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param degree Degree value to evaluate.
 *
 * @return Cosine of the angle.
 */
template<floating_point Type>
constexpr auto cos(const basic_degree<Type>& degree) noexcept -> Type;

/**
 * @brief Computes the cosine of a radian value.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param radian Radian value to evaluate.
 *
 * @return Cosine of the angle.
 */
template<floating_point Type>
constexpr auto cos(const basic_radian<Type>& radian) noexcept -> Type;

/**
 * @brief Computes the tangent of an angle.
 *
 * @tparam Type Floating-point storage type.
 *
 * @param angle Angle to evaluate.
 *
 * @return Tangent of the angle.
 */
template<floating_point Type>
constexpr auto tan(const basic_angle<Type>& angle) noexcept -> Type;

/**
 * @brief User-defined literals for degree and radian values.
 *
 * These literals create values using the library's default scalar type (std::float_t).
 */
namespace literals {

/**
 * @brief Creates a degree value from a long double literal.
 *
 * @param value Degree literal.
 *
 * @return Degree value.
 */
constexpr auto operator""_deg(long double value) noexcept -> degree;

/**
 * @brief Creates a degree value from an unsigned integer literal.
 *
 * @param value Degree literal.
 *
 * @return Degree value.
 */
constexpr auto operator""_deg(unsigned long long value) noexcept -> degree;

/**
 * @brief Creates a radian value from a long double literal.
 *
 * @param value Radian literal.
 *
 * @return Radian value.
 */
constexpr auto operator""_rad(long double value) noexcept -> radian;

/**
 * @brief Creates a radian value from an unsigned integer literal.
 *
 * @param value Radian literal.
 *
 * @return Radian value.
 */
constexpr auto operator""_rad(unsigned long long value) noexcept -> radian;

} // namespace literals

} // namespace sbx::math

#include <libsbx/math/angle.ipp>

#endif // LIBSBX_MATH_ANGLE_HPP_
