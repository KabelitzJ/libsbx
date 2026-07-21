#ifndef LIBSBX_UNITS_VOLUME_HPP_
#define LIBSBX_UNITS_VOLUME_HPP_

#include <libsbx/units/quantity.hpp>

namespace sbx::units {

using cubic_meters = quantity<volume_dimension, std::float_t>;

template<>
struct unit_formatter<volume_dimension, std::ratio<1>> {
  static constexpr auto symbol = "m^3";
}; // struct unit_formatter

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
