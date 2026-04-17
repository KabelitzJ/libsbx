// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UNITS_BYTES_HPP_
#define LIBSBX_UNITS_BYTES_HPP_

#include <libsbx/units/quantity.hpp>

namespace sbx::units {

namespace detail {

struct byte_tag { };

} // namespace detail

using kibi = std::ratio<1024>;
using mebi = std::ratio<1024 * 1024>;
using gibi = std::ratio<1024 * 1024 * 1024>;

using gibibyte = quantity<detail::byte_tag, std::uint64_t, gibi>;
using gigabyte = quantity<detail::byte_tag, std::uint64_t, std::giga>;
using mebibyte = quantity<detail::byte_tag, std::uint64_t, mebi>;
using megabyte = quantity<detail::byte_tag, std::uint64_t, std::mega>;
using kibibyte = quantity<detail::byte_tag, std::uint64_t, kibi>;
using kilobyte = quantity<detail::byte_tag, std::uint64_t, std::kilo>;
using byte = quantity<detail::byte_tag, std::uint64_t>;

namespace literals {

constexpr auto operator""_gib(unsigned long long value) -> gibibyte {
  return gibibyte{value};
}

constexpr auto operator""_gb(unsigned long long value) -> gigabyte {
  return gigabyte{value};
}

constexpr auto operator""_mib(unsigned long long value) -> mebibyte {
  return mebibyte{value};
}

constexpr auto operator""_mb(unsigned long long value) -> megabyte {
  return megabyte{value};
}

constexpr auto operator""_kib(unsigned long long value) -> kibibyte {
  return kibibyte{value};
}

constexpr auto operator""_kb(unsigned long long value) -> kilobyte {
  return kilobyte{value};
}

constexpr auto operator""_b(unsigned long long value) -> byte {
  return byte{value};
}

} // namespace literals

} // namespace sbx::units

#endif // LIBSBX_UNITS_BYTES_HPP_
