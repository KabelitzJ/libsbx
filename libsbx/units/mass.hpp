#ifndef LIBSBX_UNITS_MASS_HPP_
#define LIBSBX_UNITS_MASS_HPP_

#include <ratio>

#include <libsbx/units/quantity.hpp>

namespace sbx::units {

using grams = quantity<mass_dimension, std::float_t, std::milli>;
using kilograms = quantity<mass_dimension, std::float_t>;

template<>
struct unit_formatter<mass_dimension, std::milli> {
  static constexpr auto symbol = "g";
}; // struct unit_formatter

template<>
struct unit_formatter<mass_dimension, std::ratio<1>> {
  static constexpr auto symbol = "kg";
}; // struct unit_formatter

namespace literals {

constexpr auto operator""_kg(long double value) -> kilograms {
  return kilograms{static_cast<typename kilograms::representation_type>(value)};
}

constexpr auto operator""_kg(unsigned long long int value) -> kilograms {
  return kilograms{static_cast<typename kilograms::representation_type>(value)};
}

} // namespace literals

} // namespace sbx::units

#endif // LIBSBX_UNITS_MASS_HPP_
