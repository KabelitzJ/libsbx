#ifndef LIBSBX_UNITS_VELOCITY_HPP_
#define LIBSBX_UNITS_VELOCITY_HPP_

#include <libsbx/units/quantity.hpp>

namespace sbx::units {

using meters_per_second = quantity<velocity_dimension, std::float_t>;

template<>
struct unit_formatter<velocity_dimension, std::ratio<1>> {
  static constexpr auto symbol = "m/s";
}; // struct unit_formatter

} // namespace sbx::units

#endif // LIBSBX_UNITS_VELOCITY_HPP_
