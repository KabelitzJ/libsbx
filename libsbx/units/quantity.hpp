#ifndef LIBSBX_UNITS_QUANTITY_HPP_
#define LIBSBX_UNITS_QUANTITY_HPP_

#include <cmath>

#include <fmt/format.h>

#include <libsbx/units/dimension.hpp>

namespace sbx::units {

namespace detail {

template<typename ToScale, typename FromScale, typename ToRepresentation, typename FromRepresentation>
constexpr auto convert_value(const FromRepresentation value) -> ToRepresentation {
  using factor = std::ratio_divide<FromScale, ToScale>;

  return static_cast<ToRepresentation>(value) * (static_cast<ToRepresentation>(factor::num) / static_cast<ToRepresentation>(factor::den));
}

} // namespace detail

template<typename Dimension, typename Representation, typename Scale = std::ratio<1>>
class quantity {

public:

  using dimension_type = Dimension;
  using representation_type = Representation;
  using scale = Scale;

  constexpr quantity() = default;

  explicit constexpr quantity(const representation_type value)
  : _value{value} { }

  template<typename OtherRepresentation, typename OtherScale>
  constexpr quantity(const quantity<dimension_type, OtherRepresentation, OtherScale>& other)
  : _value{detail::convert_value<scale, OtherScale, representation_type>(static_cast<representation_type>(other.value()))} { }

  constexpr auto value() const -> representation_type {
    return _value;
  }

  constexpr operator representation_type() const {
    return value();
  }

  template<typename OtherRepresentation, typename OtherScale>
  constexpr auto operator+=(const quantity<dimension_type, OtherRepresentation, OtherScale>& other) -> quantity& {
    _value += detail::convert_value<scale, OtherScale, representation_type>(other.value());

    return *this;
  }

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

template<typename LhsDimension, typename LhsRepresentation, typename LhsScale, typename RhsRepresentation, typename RhsDimension, typename RhsScale>
constexpr auto operator*(quantity<LhsDimension, LhsRepresentation, LhsScale> lhs, quantity<RhsDimension, RhsRepresentation, RhsScale> rhs) -> quantity<simplify_dimension_t<dimension_multiply<LhsDimension, RhsDimension>>, std::common_type_t<LhsRepresentation, RhsRepresentation>, std::ratio_multiply<LhsScale, RhsScale>> {
  using representation_type = std::common_type_t<LhsRepresentation, RhsRepresentation>;
  using dimension_type = simplify_dimension_t<dimension_multiply<LhsDimension, RhsDimension>>;

  return quantity<dimension_type, representation_type, std::ratio_multiply<LhsScale, RhsScale>>{static_cast<representation_type>(lhs.value()) * static_cast<representation_type>(rhs.value())};
}

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

template<typename ToQuantity, typename Dimension, typename Representation, typename Scale>
constexpr auto quantity_cast(const quantity<Dimension, Representation, Scale>& value) -> ToQuantity {
  return ToQuantity{value};
}

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
