// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UNITS_MASS_HPP_
#define LIBSBX_UNITS_MASS_HPP_

#include <libsbx/units/quantity.hpp>

namespace sbx::units {

namespace detail {

struct mass_tag { };

} // namespace detail

using kilogram = quantity<detail::mass_tag, std::float_t, std::kilo>;
using gram = quantity<detail::mass_tag, std::float_t>;
using milligram = quantity<detail::mass_tag, std::float_t, std::milli>;

namespace literals {

constexpr auto operator""_kg(long double value) -> kilogram {
  return kilogram{static_cast<kilogram::value_type>(value)};
}

constexpr auto operator""_kg(unsigned long long value) -> kilogram {
  return kilogram{static_cast<kilogram::value_type>(value)};
}

constexpr auto operator""_g(long double value) -> gram {
  return gram{static_cast<gram::value_type>(value)};
}

constexpr auto operator""_g(unsigned long long value) -> gram {
  return gram{static_cast<gram::value_type>(value)};
}

constexpr auto operator""_mg(long double value) -> milligram {
  return milligram{static_cast<milligram::value_type>(value)};
}

constexpr auto operator""_mg(unsigned long long value) -> milligram {
  return milligram{static_cast<milligram::value_type>(value)};
}

} // namespace literals

} // namespace sbx::units

#endif // LIBSBX_UNITS_MASS_HPP_
