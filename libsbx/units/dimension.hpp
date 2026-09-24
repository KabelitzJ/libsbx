// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UNITS_DIMENSION_HPP_
#define LIBSBX_UNITS_DIMENSION_HPP_

#include <ratio>
#include <type_traits>
#include <compare>

namespace sbx::units {

/**
 * @brief An SI dimension: the exponent of each of the seven SI base units, as std::ratio.
 *
 * @tparam Length The length (metre) exponent.
 * @tparam Mass The mass (kilogram) exponent.
 * @tparam Time The time (second) exponent.
 * @tparam Current The electric current (ampere) exponent.
 * @tparam Temperature The thermodynamic temperature (kelvin) exponent.
 * @tparam Amount The amount of substance (mole) exponent.
 * @tparam Luminosity The luminous intensity (candela) exponent.
 */
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

/**
 * @brief The dimension of a product of two quantities: each exponent of Lhs and Rhs added together.
 *
 * @tparam Lhs The left-hand dimension.
 * @tparam Rhs The right-hand dimension.
 */
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

/**
 * @brief The dimension of a quotient of two quantities: each exponent of Rhs subtracted from Lhs.
 *
 * @tparam Lhs The left-hand (numerator) dimension.
 * @tparam Rhs The right-hand (denominator) dimension.
 */
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

/**
 * @brief Reduces a dimension_multiply/dimension_division result to its named dimension, where one is known (e.g. length * length to area_dimension). Falls through to the unreduced type when no named dimension matches.
 *
 * @tparam Dimension The dimension to reduce.
 *
 * @note This is a lookup table of known reductions, not general canonicalization: named dimensions (length_dimension, area_dimension, ...) are distinct types from the dimension_multiply/dimension_division that produced them (so that compiler diagnostics and debuggers show "area_dimension" instead of a raw ratio-exponent soup), so each reduction needs its own specialization below rather than falling out automatically. A product/quotient with no specialization here stays an unreduced dimension_multiply<...>/dimension_division<...> type — still dimensionally correct, just without a friendly name.
 */
template<typename Dimension>
struct simplify_dimension {
  using type = Dimension;
}; // struct simplify_dimension

/** @brief See simplify_dimension. */
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

template<>
struct simplify_dimension<dimension_division<length_dimension, length_dimension>> {
  using type = dimensionless;
}; // struct simplify_dimension

template<>
struct simplify_dimension<dimension_division<mass_dimension, mass_dimension>> {
  using type = dimensionless;
}; // struct simplify_dimension

template<>
struct simplify_dimension<dimension_division<time_dimension, time_dimension>> {
  using type = dimensionless;
}; // struct simplify_dimension

template<>
struct simplify_dimension<dimension_division<area_dimension, area_dimension>> {
  using type = dimensionless;
}; // struct simplify_dimension

template<>
struct simplify_dimension<dimension_division<volume_dimension, volume_dimension>> {
  using type = dimensionless;
}; // struct simplify_dimension

template<>
struct simplify_dimension<dimension_division<velocity_dimension, velocity_dimension>> {
  using type = dimensionless;
}; // struct simplify_dimension

} // namespace sbx::units

#endif // LIBSBX_UNITS_DIMENSION_HPP_
