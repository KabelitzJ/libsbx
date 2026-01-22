// SPDX-License-Identifier: MIT
/**
 * @file libsbx/math/constants.hpp
 * 
 * @brief Mathematical constants.
 *
 * @details
 * 
 * This header defines commonly used mathematical constants in a type-safe,
 * constexpr-friendly manner. Constants are provided as variable templates
 * parameterized on floating-point type, along with convenient aliases for
 * commonly used precisions.
 *
 * All constants are intended to be used consistently across the math module
 * to avoid magic numbers and precision mismatches.
 * 
 * @author KAJ
 *
 * @copyright (C) 2026 Jonas Kabelitz
 *
 * @package libsbx::math
 * @version 0.1.0
 * @since 2025-07-14
 */

#ifndef LIBSBX_MATH_CONSTANTS_HPP_
#define LIBSBX_MATH_CONSTANTS_HPP_

#include <cstdint>
#include <cmath>
#include <limits>
#include <numbers>

#include <libsbx/math/concepts.hpp>

namespace sbx::math {

/**
 * @brief Machine epsilon for a floating-point type.
 *
 * @tparam Type Floating-point type.
 */
template<floating_point Type>
inline constexpr auto epsilon_v = std::numeric_limits<Type>::epsilon();

/**
 * @brief Machine epsilon for std::float_t.
 */
inline constexpr auto epsilonf = epsilon_v<std::float_t>;

/**
 * @brief Machine epsilon for std::double_t.
 */
inline constexpr auto epsilond = epsilon_v<std::double_t>;

/**
 * @brief Default machine epsilon.
 *
 * Alias for @ref epsilonf.
 */
inline constexpr auto epsilon = epsilonf;

/**
 * @brief Pi constant for a floating-point type.
 *
 * @tparam Type Floating-point type.
 */
template<floating_point Type>
inline constexpr auto pi_v = std::numbers::pi_v<Type>;

/**
 * @brief Pi constant for std::float_t.
 */
inline constexpr auto pif = pi_v<std::float_t>;

/**
 * @brief Pi constant for std::double_t.
 */
inline constexpr auto pid = pi_v<std::double_t>;

/**
 * @brief Default pi constant.
 *
 * Alias for @ref pif.
 */
inline constexpr auto pi = pif;

/**
 * @brief Two-pi constant for a floating-point type.
 *
 * @tparam Type Floating-point type.
 */
template<floating_point Type>
inline constexpr auto two_pi_v = static_cast<Type>(2) * pi_v<Type>;

/**
 * @brief Two-pi constant for std::float_t.
 */
inline constexpr auto two_pif = two_pi_v<std::float_t>;

/**
 * @brief Two-pi constant for std::double_t.
 */
inline constexpr auto two_pid = two_pi_v<std::double_t>;

/**
 * @brief Default two-pi constant.
 *
 * Alias for @ref two_pif.
 */
inline constexpr auto two_pi = two_pif;

} // namespace sbx::math

#endif // LIBSBX_MATH_CONSTANTS_HPP_
