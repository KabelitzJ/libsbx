// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UNITS_VOLUME_HPP_
#define LIBSBX_UNITS_VOLUME_HPP_

#include <libsbx/units/quantity.hpp>

namespace sbx::units {

/** @brief Volume quantities, in cubic metres. */
using cubic_meters = quantity<volume_dimension, std::float_t>;

template<>
struct unit_formatter<volume_dimension, std::ratio<1>> {
  static constexpr auto symbol = "m^3";
}; // struct unit_formatter

/** @brief Literal suffix for constructing volume quantities directly, e.g. `2.0_m3`. */
namespace literals {

constexpr auto operator""_m3(long double value) -> cubic_meters {
  return cubic_meters{static_cast<typename cubic_meters::representation_type>(value)};
}

constexpr auto operator""_m3(unsigned long long int value) -> cubic_meters {
  return cubic_meters{static_cast<typename cubic_meters::representation_type>(value)};
}

} // namespace literals

} // namespace sbx::units

#endif // LIBSBX_UNITS_VOLUME_HPP_
