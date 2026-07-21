#ifndef LIBSBX_UNITS_DIMENSION_HPP_
#define LIBSBX_UNITS_DIMENSION_HPP_

#include <ratio>
#include <type_traits>
#include <compare>

namespace sbx::units {

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
struct dimension_multiply : dimension<
  std::ratio_add<typename Lhs::length, typename Rhs::length>,
  std::ratio_add<typename Lhs::mass, typename Rhs::mass>,
  std::ratio_add<typename Lhs::time, typename Rhs::time>,
  std::ratio_add<typename Lhs::current, typename Rhs::current>,
  std::ratio_add<typename Lhs::temperature, typename Rhs::temperature>,
  std::ratio_add<typename Lhs::amount, typename Rhs::amount>,
  std::ratio_add<typename Lhs::luminosity, typename Rhs::luminosity>
> { }; // struct dimension_multiply

template<typename Lhs, typename Rhs>
struct dimension_division : dimension<
  std::ratio_subtract<typename Lhs::length, typename Rhs::length>,
  std::ratio_subtract<typename Lhs::mass, typename Rhs::mass>,
  std::ratio_subtract<typename Lhs::time, typename Rhs::time>,
  std::ratio_subtract<typename Lhs::current, typename Rhs::current>,
  std::ratio_subtract<typename Lhs::temperature, typename Rhs::temperature>,
  std::ratio_subtract<typename Lhs::amount, typename Rhs::amount>,
  std::ratio_subtract<typename Lhs::luminosity, typename Rhs::luminosity>
> { }; // struct dimension_division

template<typename Dimension>
struct simplify_dimension {
  using type = Dimension;
}; // struct simplify_dimension

template<typename Lhs, typename Rhs>
struct simplify_dimension<dimension_multiply<Lhs, Rhs>> {
  using type = dimension_multiply<Lhs, Rhs>;
}; // simplify_dimension

template<typename Dimension>
using simplify_dimension_t = typename simplify_dimension<Dimension>::type;

struct dimensionless : dimension<std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>> { };
struct length_dimension : dimension<std::ratio<1>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>> { };
struct mass_dimension : dimension<std::ratio<0>, std::ratio<1>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>> { };
struct time_dimension : dimension<std::ratio<0>, std::ratio<0>, std::ratio<1>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>> { };
struct current_dimension : dimension<std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<1>, std::ratio<0>, std::ratio<0>, std::ratio<0>> { };
struct temperature_dimension : dimension<std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<0>, std::ratio<1>, std::ratio<0>, std::ratio<0>> { };

struct area_dimension : dimension_multiply<length_dimension, length_dimension> { };
struct volume_dimension : dimension_multiply<area_dimension, length_dimension> { };
struct velocity_dimension : dimension_division<length_dimension, time_dimension> { };
struct acceleration_dimension : dimension_division<velocity_dimension, time_dimension> { };

template<>
struct simplify_dimension<dimension_multiply<length_dimension, length_dimension>> {
  using type = area_dimension;
}; // struct simplify_dimension

template<>
struct simplify_dimension<dimension_multiply<area_dimension, length_dimension>> {
  using type = volume_dimension;
}; // struct simplify_dimension

template<>
struct simplify_dimension<dimension_division<area_dimension, length_dimension>> {
  using type = length_dimension;
}; // struct simplify_dimension

template<>
struct simplify_dimension<dimension_division<volume_dimension, length_dimension>> {
  using type = area_dimension;
}; // struct simplify_dimension

template<>
struct simplify_dimension<dimension_division<length_dimension, time_dimension>> {
  using type = velocity_dimension;
}; // struct simplify_dimension

template<>
struct simplify_dimension<dimension_division<velocity_dimension, time_dimension>> {
  using type = acceleration_dimension;
}; // struct simplify_dimension

} // namespace sbx::units

#endif // LIBSBX_UNITS_DIMENSION_HPP_
