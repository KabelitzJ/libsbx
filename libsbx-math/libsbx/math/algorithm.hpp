// SPDX-License-Identifier: MIT
/**
 * @file algorithm.hpp
 *
 * @brief Generic math algorithms and helpers.
 *
 * This header provides small, constexpr-friendly utilities for common mathematical operations
 * on floating-point types. Implementations are provided in the accompanying .ipp file.
 */

#ifndef LIBSBX_MATH_ALGORITHM_HPP_
#define LIBSBX_MATH_ALGORITHM_HPP_

#include <cmath>
#include <limits>

#include <libsbx/math/concepts.hpp>

namespace sbx::math {

/**
 * @brief Linearly interpolates between two values.
 *
 * Computes `x * (1 - a) + y * a`.
 *
 * @tparam Type Floating-point type.
 *
 * @param x Start value.
 *
 * @param y End value.
 *
 * @param a Interpolation factor.
 *
 * @return Interpolated value.
 */
template<floating_point Type>
inline constexpr auto mix(const Type x, const Type y, const Type a) -> Type;

/**
 * @brief Computes the absolute value.
 *
 * This function forwards to `std::abs`.
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
 * @brief Computes the square root using iterative refinement.
 *
 * This implementation uses Newton iteration to approximate the square root.
 *
 * For negative inputs and positive infinity, this function returns `quiet_NaN()`.
 *
 * @tparam Type Floating-point type.
 *
 * @param value Input value.
 *
 * @return Square root approximation, or NaN for invalid inputs.
 */
template<floating_point Type>
inline constexpr auto sqrt(const Type value) -> Type;

} // namespace sbx::math

#include <libsbx/math/algorithm.ipp>

#endif // LIBSBX_MATH_ALGORITHM_HPP_
