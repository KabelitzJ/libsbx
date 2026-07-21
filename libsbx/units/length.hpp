#ifndef LIBSBX_UNITS_LENGTH_HPP_
#define LIBSBX_UNITS_LENGTH_HPP_

#include <ratio>

#include <libsbx/units/quantity.hpp>

namespace sbx::units {

using millimeters = quantity<length_dimension, std::float_t, std::milli>;
using centimeters = quantity<length_dimension, std::float_t, std::centi>;
using meters = quantity<length_dimension, std::float_t>;
using kilometers = quantity<length_dimension, std::float_t, std::kilo>;

template<>
struct unit_formatter<length_dimension, std::milli> {
  static constexpr auto symbol = "mm";
}; // struct unit_formatter

template<>
struct unit_formatter<length_dimension, std::centi> {
  static constexpr auto symbol = "cm";
}; // struct unit_formatter

template<>
struct unit_formatter<length_dimension, std::ratio<1>> {
  static constexpr auto symbol = "m";
}; // struct unit_formatter

template<>
struct unit_formatter<length_dimension, std::kilo> {
  static constexpr auto symbol = "km";
}; // struct unit_formatter

namespace literals {

constexpr auto operator""_m(long double value) -> meters {
  return meters{static_cast<typename meters::representation_type>(value)};
}

constexpr auto operator""_m(unsigned long long int value) -> meters {
  return meters{static_cast<typename meters::representation_type>(value)};
}

constexpr auto operator""_km(long double value) -> kilometers {
  return kilometers{static_cast<typename kilometers::representation_type>(value)};
}

constexpr auto operator""_km(unsigned long long int value) -> kilometers {
  return kilometers{static_cast<typename kilometers::representation_type>(value)};
}

constexpr auto operator""_cm(long double value) -> centimeters {
  return centimeters{static_cast<typename centimeters::representation_type>(value)};
}

constexpr auto operator""_cm(unsigned long long int value) -> centimeters {
  return centimeters{static_cast<typename centimeters::representation_type>(value)};
}

} // namespace literals

} // namespace sbx::units

#endif // LIBSBX_UNITS_LENGTH_HPP_
