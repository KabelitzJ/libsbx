// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_FLAGS_HPP_
#define LIBSBX_UTILITY_FLAGS_HPP_

#include <concepts>
#include <type_traits>

namespace sbx::utility {

template<std::size_t Shift>
struct bit : std::integral_constant<std::size_t, (std::size_t{1} << Shift)> { };

template<std::size_t Shift>
inline constexpr auto bit_v = bit<Shift>::value;

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_FLAGS_HPP_
