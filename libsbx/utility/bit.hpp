// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_BIT_HPP_
#define LIBSBX_UTILITY_BIT_HPP_

#include <concepts>
#include <type_traits>

namespace sbx::utility {

/**
 * @brief A single set bit, for building bitmask enums/constants.
 *
 * @tparam Shift The bit position to set, from the least significant bit (0).
 */
template<std::size_t Shift>
struct bit : std::integral_constant<std::size_t, (std::size_t{1} << Shift)> { };

/** @brief The value of bit<Shift>. */
template<std::size_t Shift>
inline constexpr auto bit_v = bit<Shift>::value;

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_BIT_HPP_
