// SPDX-License-Identifier: MIT
/**
 * @file libsbx/math/concepts.hpp
 * 
 * @brief Core numeric concepts and type traits.
 *
 * @details
 * 
 * This header defines foundational type traits and C++20 concepts used
 * throughout the math module. The provided concepts classify numeric,
 * scalar, integral, and floating-point types with stricter semantics
 * than the standard library equivalents.
 *
 * These utilities are intended to enforce stronger compile-time
 * constraints and improve readability of template interfaces.
 * 
 * @author KAJ
 *
 * @copyright (C) 2026 Jonas Kabelitz
 *
 * @package libsbx::math
 * @version 0.1.0
 * @since 2022-12-14
 */

#ifndef LIBSBX_MATH_CONCEPTS_HPP_
#define LIBSBX_MATH_CONCEPTS_HPP_

#include <type_traits>
#include <numbers>
#include <limits>
#include <cmath>

namespace sbx::math {

/**
 * @brief Concept for numeric types.
 *
 * @details
 * 
 * A numeric type is either:
 * - a non-boolean integral type, or
 * - a floating-point type.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
concept numeric = (std::is_integral_v<Type> && !std::is_same_v<Type, bool>) || std::is_floating_point_v<Type>;

/**
 * @brief Type trait identifying floating-point types.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
struct is_floating_point : std::bool_constant<std::is_floating_point_v<Type>> { };

/**
 * @brief Convenience variable for @ref is_floating_point.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
inline constexpr bool is_floating_point_v = is_floating_point<Type>::value;

/**
 * @brief Concept for floating-point types.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
concept floating_point = is_floating_point_v<Type>;

/**
 * @brief Type trait identifying non-boolean integral types.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
struct is_integral : std::bool_constant<std::is_integral_v<Type> && !std::is_same_v<Type, bool>> { };

/**
 * @brief Convenience variable for @ref is_integral.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
inline constexpr bool is_integral_v = is_integral<Type>::value;

/**
 * @brief Concept for non-boolean integral types.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
concept integral = is_integral_v<Type>;

/**
 * @brief Type trait identifying unsigned integral types.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
struct is_unsigned_integral : std::bool_constant<is_integral_v<Type> && std::is_unsigned_v<Type>> { };

/**
 * @brief Convenience variable for @ref is_unsigned_integral.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
inline constexpr bool is_unsigned_integral_v = is_unsigned_integral<Type>::value;

/**
 * @brief Concept for unsigned integral types.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
concept unsigned_integral = is_unsigned_integral_v<Type>;

/**
 * @brief Type trait identifying scalar types.
 *
 * @details
 * 
 * A scalar type is either:
 * - a floating-point type, or
 * - a non-boolean integral type.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
struct is_scalar : std::bool_constant<is_floating_point_v<Type> || is_integral_v<Type>> { };

/**
 * @brief Convenience variable for @ref is_scalar.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
inline constexpr bool is_scalar_v = is_scalar<Type>::value;

/**
 * @brief Concept for scalar numeric types.
 *
 * @tparam Type Type to test.
 */
template<typename Type>
concept scalar = is_scalar_v<Type>;

} // namespace sbx::math

#endif // LIBSBX_MATH_CONCEPTS_HPP_
