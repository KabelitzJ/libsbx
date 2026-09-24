// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/math/algorithm.hpp
 *
 * @brief Generic constexpr math algorithms for floating-point types.
 *
 * @ingroup libsbx-math
 */

#ifndef LIBSBX_MATH_ALGORITHM_HPP_
#define LIBSBX_MATH_ALGORITHM_HPP_

#include <cmath>
#include <limits>

#include <libsbx/math/concepts.hpp>

namespace sbx::math {

/**
 * @brief Linearly interpolates between two values. Computes `x * (1 - a) + y * a`.
 *
 * @tparam Type Floating-point type.
 *
 * @param x Start value.
 * @param y End value.
 * @param a Interpolation factor.
 *
 * @return Interpolated value.
 */
template<floating_point Type>
inline constexpr auto mix(const Type x, const Type y, const Type a) -> Type;

/**
 * @brief Computes the absolute value. Forwards to `std::abs`.
 *
 * @tparam Type Floating-point type.
 *
 * @param value Input value.
 *
 * @return Absolute value of the input.
 */
template<floating_point Type>
inline constexpr auto abs(const Type value) -> Type;

/**
 * @brief Computes the square root via Newton iteration (Heron's method), capped at 64 iterations as a termination safeguard against floating-point oscillation near the root — the loop otherwise exits as soon as an iteration stops changing the result.
 *
 * @tparam Type Floating-point type.
 *
 * @param value Input value.
 *
 * @return The square root of value; `quiet_NaN()` for value < 0. value itself is returned unchanged for 0 (either sign) and positive infinity, matching `std::sqrt`.
 */
template<floating_point Type>
inline constexpr auto sqrt(const Type value) -> Type;

} // namespace sbx::math

#include <libsbx/math/algorithm.ipp>

#endif // LIBSBX_MATH_ALGORITHM_HPP_
