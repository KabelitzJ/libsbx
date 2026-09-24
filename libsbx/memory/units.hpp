// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MEMORY_UNITS_HPP_
#define LIBSBX_MEMORY_UNITS_HPP_

#include <cstddef>
#include <ratio>
#include <type_traits>
#include <utility>

namespace sbx::memory {

/**
 * @brief Value expressed in Ratio units, converted to bytes.
 *
 * @tparam Value The value, in Ratio units.
 * @tparam Ratio The unit; must have a denominator of 1 (see byte/kib/mib/gib).
 */
template<std::size_t Value, typename Ratio>
requires (Ratio::den == 1)
struct unit : std::integral_constant<std::size_t, (Value * Ratio::num)> { };

template<std::size_t Value, typename Ratio>
requires (Ratio::den == 1)
inline constexpr auto unit_v = unit<Value, Ratio>::value;

using byte = std::ratio<1>;
using kib = std::ratio<1024>;
using mib = std::ratio<1024 * 1024>;
using gib = std::ratio<1024 * 1024 * 1024>;

/** @brief Value bytes, in bytes. */
template<std::size_t Value>
inline constexpr auto byte_v = unit_v<Value, byte>;

/** @brief Value KiB, in bytes. */
template<std::size_t Value>
inline constexpr auto kib_v = unit_v<Value, kib>;

/** @brief Value MiB, in bytes. */
template<std::size_t Value>
inline constexpr auto mib_v = unit_v<Value, mib>;

/** @brief Value GiB, in bytes. */
template<std::size_t Value>
inline constexpr auto gib_v = unit_v<Value, gib>;

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_UNITS_HPP_
