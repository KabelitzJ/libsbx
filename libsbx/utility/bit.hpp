// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_ENUM_HPP_
#define LIBSBX_UTILITY_ENUM_HPP_

#include <type_traits>
#include <array>
#include <ranges>
#include <optional>
#include <string_view>

namespace sbx::utility {

template<std::size_t Shift>
struct bit : std::integral_constant<std::size_t, (std::size_t{1} << Shift)> { };

template<std::size_t Shift>
inline constexpr auto bit_v = bit<Shift>::value;

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_ENUM_HPP_
