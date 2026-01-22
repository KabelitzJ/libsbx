// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_CONSTANTS_HPP_
#define LIBSBX_MATH_CONSTANTS_HPP_

#include <cstdint>
#include <cmath>
#include <limits>
#include <numbers>

#include <libsbx/math/concepts.hpp>

namespace sbx::math {

template<floating_point Type>
inline constexpr auto epsilon_v = std::numeric_limits<Type>::epsilon();

inline constexpr auto epsilonf = epsilon_v<std::float_t>;

inline constexpr auto epsilond = epsilon_v<std::double_t>;

inline constexpr auto epsilon = epsilonf;

template<floating_point Type>
inline constexpr auto pi_v = std::numbers::pi_v<Type>;

inline constexpr auto pif = pi_v<std::float_t>;

inline constexpr auto pid = pi_v<std::double_t>;

inline constexpr auto pi = pif;

template<floating_point Type>
inline constexpr auto two_pi_v = static_cast<Type>(2) * pi_v<Type>;

inline constexpr auto two_pif = two_pi_v<std::float_t>;

inline constexpr auto two_pid = two_pi_v<std::double_t>;

inline constexpr auto two_pi = pif;

} // namespace sbx::math

#endif // LIBSBX_MATH_CONSTANTS_HPP_
