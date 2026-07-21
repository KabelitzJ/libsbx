#ifndef LIBSBX_UNITS_AREA_HPP_
#define LIBSBX_UNITS_AREA_HPP_

#include <libsbx/units/quantity.hpp>

namespace sbx::units {

using square_meters = quantity<area_dimension, std::float_t>;

template<>
struct unit_formatter<area_dimension, std::ratio<1>> {
  static constexpr auto symbol = "m^2";
}; // struct unit_formatter

namespace literals {

constexpr auto operator""_m2(long double value) -> square_meters {
  return square_meters{static_cast<typename square_meters::representation_type>(value)};
}

constexpr auto operator""_m2(unsigned long long int value) -> square_meters {
  return square_meters{static_cast<typename square_meters::representation_type>(value)};
}

} // namespace literals

} // namespace sbx::units

#endif // LIBSBX_UNITS_AREA_HPP_
