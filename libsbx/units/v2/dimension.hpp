#ifndef LIBSBX_UNITS_V2_DIMENSION_HPP_
#define LIBSBX_UNITS_V2_DIMENSION_HPP_

#include <ratio>
#include <type_traits>
#include <compare>

namespace sbx::units::v2 {

template <typename Length, typename Mass, typename Time, typename Current, typename Temperature, typename Amount, typename Luminosity>
struct dimension {
  using length = Length;
  using mass = Mass;
  using time = Time;
  using current = Current;
  using temperature = Temperature;
  using amount = Amount;
  using luminosity = Luminosity;
}; // struct dimension

template<typename Lhs, typename Rhs>
using dimension_multiply = dimension<
  std::ratio_add<typename Lhs::length, typename Rhs::length>,
  std::ratio_add<typename Lhs::mass, typename Rhs::mass>,
  std::ratio_add<typename Lhs::time, typename Rhs::time>,
  std::ratio_add<typename Lhs::current, typename Rhs::current>,
  std::ratio_add<typename Lhs::temperature, typename Rhs::temperature>,
  std::ratio_add<typename Lhs::amount, typename Rhs::amount>,
  std::ratio_add<typename Lhs::luminosity, typename Rhs::luminosity>
>;

template<typename Lhs, typename Rhs>
using dimension_division = dimension<
  std::ratio_subtract<typename Lhs::length, typename Rhs::length>,
  std::ratio_subtract<typename Lhs::mass, typename Rhs::mass>,
  std::ratio_subtract<typename Lhs::time, typename Rhs::time>,
  std::ratio_subtract<typename Lhs::current, typename Rhs::current>,
  std::ratio_subtract<typename Lhs::temperature, typename Rhs::temperature>,
  std::ratio_subtract<typename Lhs::amount, typename Rhs::amount>,
  std::ratio_subtract<typename Lhs::luminosity, typename Rhs::luminosity>
>;

using dimensionless = dimension<std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>>;
using length_dimension = dimension<std::ratio<1>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>>;
using mass_dimension = dimension<std::ratio<0>, std::ratio<1>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>>;
using time_dimension = dimension<std::ratio<0>, std::ratio<0>, std::ratio<1>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>>;
using current_dimension = dimension<std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<1>, std::ratio<0>, std::ratio<0>, std::ratio<0>>;
using temperature_dimension = dimension<std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<1>, std::ratio<0>, std::ratio<0>>;

using area_dimension = dimension_multiply<length_dimension, length_dimension>;
using volume_dimension = dimension_multiply<area_dimension, length_dimension>;
using velocity_dimension = dimension_division<length_dimension, time_dimension>;
using acceleration_dimension = dimension_division<velocity_dimension, time_dimension>;

} // namespace sbx::units::v2

#endif // LIBSBX_UNITS_V2_DIMENSION_HPP_