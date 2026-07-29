#ifndef LIBSBX_MEMORY_UNITS_HPP_
#define LIBSBX_MEMORY_UNITS_HPP_

#include <cstddef>
#include <ratio>
#include <type_traits>
#include <utility>

namespace sbx::memory {

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

template<std::size_t Value>
inline constexpr auto byte_v = unit_v<Value, byte>;

template<std::size_t Value>
inline constexpr auto kib_v = unit_v<Value, kib>;

template<std::size_t Value>
inline constexpr auto mib_v = unit_v<Value, mib>;

template<std::size_t Value>
inline constexpr auto gib_v = unit_v<Value, gib>;

} // namespace sbx::memory

#endif // LIBSBX_MEMORY_UNITS_HPP_