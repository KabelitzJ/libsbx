// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UNITS_QUANTITY_HPP_
#define LIBSBX_UNITS_QUANTITY_HPP_

#include <cmath>

#include <fmt/format.h>

#include <libsbx/units/dimension.hpp>

namespace sbx::units {

namespace detail {

/**
 * @brief Converts a value from one scale to another, within the same dimension.
 *
 * @tparam ToScale The scale to convert to.
 * @tparam FromScale The scale to convert from.
 * @tparam ToRepresentation The representation type to return.
 * @tparam FromRepresentation The representation type of value.
 *
 * @param value The value to convert, in FromScale units.
 *
 * @return value, converted to ToScale units.
 */
template<typename ToScale, typename FromScale, typename ToRepresentation, typename FromRepresentation>
constexpr auto convert_value(const FromRepresentation value) -> ToRepresentation {
  using factor = std::ratio_divide<FromScale, ToScale>;

  return static_cast<ToRepresentation>(value) * (static_cast<ToRepresentation>(factor::num) / static_cast<ToRepresentation>(factor::den));
}

} // namespace detail

/**
 * @brief A scalar value tagged with an SI dimension, e.g. a length or a duration. See units.hpp's
 * per-dimension headers (length.hpp, time.hpp, ...) for the concrete aliases and literals.
 *
 * @tparam Dimension The quantity's dimension (see dimension.hpp).
 * @tparam Representation The underlying scalar type.
 * @tparam Scale The unit scale relative to the dimension's SI base unit, as a std::ratio. Defaults
 * to std::ratio<1> (the base unit itself, e.g. metres for length).
 */
template<typename Dimension, typename Representation, typename Scale = std::ratio<1>>
class quantity {

public:

  using dimension_type = Dimension;
  using representation_type = Representation;
  using scale = Scale;

  constexpr quantity() = default;

  /**
   * @brief Constructs from a raw value, in this quantity's own Scale.
   *
   * @param value The value, in Scale units.
   */
  explicit constexpr quantity(const representation_type value)
  : _value{value} { }

  /**
   * @brief Converting constructor from a quantity of the same dimension but a different
   * representation and/or scale (e.g. milliseconds to seconds).
   *
   * @tparam OtherRepresentation The other quantity's representation type.
   * @tparam OtherScale The other quantity's scale.
   *
   * @param other The quantity to convert from.
   */
  template<typename OtherRepresentation, typename OtherScale>
  constexpr quantity(const quantity<dimension_type, OtherRepresentation, OtherScale>& other)
  : _value{detail::convert_value<scale, OtherScale, representation_type>(static_cast<representation_type>(other.value()))} { }

  /** @return The raw value, in this quantity's own Scale. */
  constexpr auto value() const -> representation_type {
    return _value;
  }

  constexpr operator representation_type() const {
    return value();
  }

  /**
   * @brief Adds other, converting it to this quantity's scale first if needed.
   *
   * @tparam OtherRepresentation The other quantity's representation type.
   * @tparam OtherScale The other quantity's scale.
   *
   * @param other The quantity to add.
   *
   * @return *this.
   */
  template<typename OtherRepresentation, typename OtherScale>
  constexpr auto operator+=(const quantity<dimension_type, OtherRepresentation, OtherScale>& other) -> quantity& {
    _value += detail::convert_value<scale, OtherScale, representation_type>(other.value());

    return *this;
  }

  /**
   * @brief Subtracts other, converting it to this quantity's scale first if needed.
   *
   * @tparam OtherRepresentation The other quantity's representation type.
   * @tparam OtherScale The other quantity's scale.
   *
   * @param other The quantity to subtract.
   *
   * @return *this.
   */
  template<typename OtherRepresentation, typename OtherScale>
  constexpr auto operator-=(const quantity<dimension_type, OtherRepresentation, OtherScale>& other) -> quantity& {
    _value -= detail::convert_value<scale, OtherScale, representation_type>(other.value());

    return *this;
  }

private:

  representation_type _value{};

}; // class quantity

template<typename Dimension, typename LhsRepresentation, typename LhsScale, typename RhsRepresentation, typename RhsScale>
constexpr auto operator+(quantity<Dimension, LhsRepresentation, LhsScale> lhs, quantity<Dimension, RhsRepresentation, RhsScale> rhs) -> quantity<Dimension, std::common_type_t<LhsRepresentation, RhsRepresentation>, LhsScale> {
  using representation_type = std::common_type_t<LhsRepresentation, RhsRepresentation>;

  auto result = quantity<Dimension, representation_type, LhsScale>{lhs};
  result += rhs;

  return result;
}

template<typename Dimension, typename LhsRepresentation, typename LhsScale, typename RhsRepresentation, typename RhsScale>
constexpr auto operator-(quantity<Dimension, LhsRepresentation, LhsScale> lhs, quantity<Dimension, RhsRepresentation, RhsScale> rhs) -> quantity<Dimension, std::common_type_t<LhsRepresentation, RhsRepresentation>, LhsScale> {
  using representation_type = std::common_type_t<LhsRepresentation, RhsRepresentation>;

  auto result = quantity<Dimension, representation_type, LhsScale>{lhs};
  result -= rhs;

  return result;
}

template<typename Dimension, typename Representation, typename Scale>
constexpr auto operator-(quantity<Dimension, Representation, Scale> lhs) -> quantity<Dimension, Representation, Scale> {
  return quantity<Dimension, Representation, Scale>{-lhs.value()};
}

/** @brief Multiplies two quantities, producing a quantity in the resulting (simplified, where known) dimension. */
template<typename LhsDimension, typename LhsRepresentation, typename LhsScale, typename RhsRepresentation, typename RhsDimension, typename RhsScale>
constexpr auto operator*(quantity<LhsDimension, LhsRepresentation, LhsScale> lhs, quantity<RhsDimension, RhsRepresentation, RhsScale> rhs) -> quantity<simplify_dimension_t<dimension_multiply<LhsDimension, RhsDimension>>, std::common_type_t<LhsRepresentation, RhsRepresentation>, std::ratio_multiply<LhsScale, RhsScale>> {
  using representation_type = std::common_type_t<LhsRepresentation, RhsRepresentation>;
  using dimension_type = simplify_dimension_t<dimension_multiply<LhsDimension, RhsDimension>>;

  return quantity<dimension_type, representation_type, std::ratio_multiply<LhsScale, RhsScale>>{static_cast<representation_type>(lhs.value()) * static_cast<representation_type>(rhs.value())};
}

/** @brief Divides two quantities, producing a quantity in the resulting (simplified, where known) dimension. */
template<typename LhsDimension, typename LhsRepresentation, typename LhsScale, typename RhsRepresentation, typename RhsDimension, typename RhsScale>
constexpr auto operator/(quantity<LhsDimension, LhsRepresentation, LhsScale> lhs, quantity<RhsDimension, RhsRepresentation, RhsScale> rhs) -> quantity<simplify_dimension_t<dimension_division<LhsDimension, RhsDimension>>, std::common_type_t<LhsRepresentation, RhsRepresentation>, std::ratio_divide<LhsScale, RhsScale>> {
  using representation_type = std::common_type_t<LhsRepresentation, RhsRepresentation>;
  using dimension_type = simplify_dimension_t<dimension_division<LhsDimension, RhsDimension>>;

  return quantity<dimension_type, representation_type, std::ratio_divide<LhsScale, RhsScale>>{static_cast<representation_type>(lhs.value()) / static_cast<representation_type>(rhs.value())};
}

template<typename Dimension, typename Representation, typename Scale, typename Scalar>
constexpr auto operator*(quantity<Dimension, Representation, Scale> lhs, Scalar scale) -> quantity<Dimension, std::common_type_t<Representation, Scalar>, Scale> {
  using representation_type = std::common_type_t<Representation, Scalar>;

  return quantity<Dimension, representation_type, Scale>{static_cast<representation_type>(lhs.value()) * static_cast<representation_type>(scale)};
}

template<typename Dimension, typename Representation, typename Scale, typename Scalar>
constexpr auto operator*(Scalar scale, quantity<Dimension, Representation, Scale> lhs) -> quantity<Dimension, std::common_type_t<Representation, Scalar>, Scale> {
  using representation_type = std::common_type_t<Representation, Scalar>;

  return quantity<Dimension, representation_type, Scale>{static_cast<representation_type>(scale) * static_cast<representation_type>(lhs.value())};
}

template<typename Dimension, typename Representation, typename Scale, typename Scalar>
constexpr auto operator/(quantity<Dimension, Representation, Scale> lhs, Scalar scale) -> quantity<Dimension, std::common_type_t<Representation, Scalar>, Scale> {
  using representation_type = std::common_type_t<Representation, Scalar>;

  return quantity<Dimension, representation_type, Scale>{static_cast<representation_type>(lhs.value()) / static_cast<representation_type>(scale)};
}

template<typename Dimension, typename LhsRepresentation, typename LhsScale, typename RhsRepresentation, typename RhsScale>
constexpr auto operator==(quantity<Dimension, LhsRepresentation, LhsScale> lhs, quantity<Dimension, RhsRepresentation, RhsScale> rhs) -> bool {
  using representation_type = std::common_type_t<LhsRepresentation, RhsRepresentation>;

  return quantity<Dimension, representation_type, LhsScale>{lhs}.value() == quantity<Dimension, representation_type, LhsScale>{rhs}.value();
}

template<typename Dimension, typename LhsRepresentation, typename LhsScale, typename RhsRepresentation, typename RhsScale>
constexpr auto operator<=>(quantity<Dimension, LhsRepresentation, LhsScale> lhs, quantity<Dimension, RhsRepresentation, RhsScale> rhs) -> std::compare_three_way_result_t<std::common_type_t<LhsRepresentation, RhsRepresentation>> {
  using representation_type = std::common_type_t<LhsRepresentation, RhsRepresentation>;

  return quantity<Dimension, representation_type, LhsScale>{lhs}.value() <=> quantity<Dimension, representation_type, LhsScale>{rhs}.value();
}

/**
 * @brief Explicitly converts a quantity to a differently-scaled/represented quantity of the same dimension.
 *
 * @tparam ToQuantity The quantity type to convert to.
 * @tparam Dimension The source quantity's dimension.
 * @tparam Representation The source quantity's representation type.
 * @tparam Scale The source quantity's scale.
 *
 * @param value The quantity to convert.
 *
 * @return value, converted to ToQuantity.
 */
template<typename ToQuantity, typename Dimension, typename Representation, typename Scale>
constexpr auto quantity_cast(const quantity<Dimension, Representation, Scale>& value) -> ToQuantity {
  return ToQuantity{value};
}

/**
 * @brief The unit symbol fmt::formatter<quantity<...>> appends after the value (e.g. "m", "kg"). Specialized per dimension/scale in each unit header (length.hpp, mass.hpp, ...); the primary template's empty symbol is the fallback for dimension/scale pairs with no specialization.
 *
 * @tparam Dimension The quantity's dimension.
 * @tparam Scale The quantity's scale.
 */
template<typename Dimension, typename Scale>
struct unit_formatter {
  static constexpr auto symbol = "";
}; // unit_formatter

} // namespace sbx::units

template<typename Dimension, typename Representation, typename Scale>
struct fmt::formatter<sbx::units::quantity<Dimension, Representation, Scale>> : fmt::formatter<Representation> {

  template<typename FormatContext>
  auto format(const sbx::units::quantity<Dimension, Representation, Scale>& quantity, FormatContext& context) const -> decltype(context.out()) {
    constexpr auto format = std::is_floating_point_v<Representation> ? "{:.2f}{}" : "{}{}";

    return fmt::format_to(context.out(), format, quantity.value(), sbx::units::unit_formatter<Dimension, Scale>::symbol);
  }

}; // struct fmt::formatter

#endif // LIBSBX_UNITS_QUANTITY_HPP_
