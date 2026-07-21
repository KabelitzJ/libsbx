#ifndef LIBSBX_UNITS_QUANTITY_HPP_
#define LIBSBX_UNITS_QUANTITY_HPP_

#include <cmath>

#include <fmt/format.h>

#include <libsbx/units/dimension.hpp>

namespace sbx::units {

namespace detail {

template<typename ToScale, typename FromScale, typename Representation>
constexpr auto convert_value(const Representation value) -> Representation {
  using factor = std::ratio_divide<FromScale, ToScale>;

  return value * (static_cast<Representation>(factor::num) / static_cast<Representation>(factor::den));
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

  template<typename OtherScale>
  constexpr quantity(const quantity<dimension_type, representation_type, OtherScale>& other)
  : _value{detail::convert_value<scale, OtherScale, representation_type>(other.value())} { }

  constexpr auto value() const -> representation_type {
    return _value;
  }

  constexpr operator representation_type() const {
    return value();
  }

  template<typename OtherScale>
  constexpr auto operator+=(const quantity<dimension_type, representation_type, OtherScale>& other) -> quantity& {
    _value += detail::convert_value<scale, OtherScale, representation_type>(other.value());

    return *this;
  }

  template<typename OtherScale>
  constexpr auto operator-=(const quantity<dimension_type, representation_type, OtherScale>& other) -> quantity& {
    _value -= detail::convert_value<scale, OtherScale, representation_type>(other.value());

    return *this;
  }

private:

  representation_type _value{};

}; // class quantity

template<typename Dimension, typename Representation, typename LhsScale, typename RhsScale>
constexpr auto operator+(quantity<Dimension, Representation, LhsScale> lhs, quantity<Dimension, Representation, RhsScale> rhs) -> quantity<Dimension, Representation, LhsScale> {
  lhs += rhs;

  return lhs;
}

template<typename Dimension, typename Representation, typename LhsScale, typename RhsScale>
constexpr auto operator-(quantity<Dimension, Representation, LhsScale> lhs, quantity<Dimension, Representation, RhsScale> rhs) -> quantity<Dimension, Representation, LhsScale> {
  lhs -= rhs;

  return lhs;
}

template<typename Dimension, typename Representation, typename Scale>
constexpr auto operator-(quantity<Dimension, Representation, Scale> lhs) -> quantity<Dimension, Representation, Scale> {
  return quantity<Dimension, Representation, Scale>{-lhs.value()};
}

template<typename LhsDimension, typename RhsDimension, typename Representation, typename LhsScale, typename RhsScale>
constexpr auto operator*(quantity<LhsDimension, Representation, LhsScale> lhs, quantity<RhsDimension, Representation, RhsScale> rhs) -> quantity<dimension_multiply<LhsDimension, RhsDimension>, Representation, std::ratio_multiply<LhsScale, RhsScale>> {
  return quantity<dimension_multiply<LhsDimension, RhsDimension>, Representation, std::ratio_multiply<LhsScale, RhsScale>>{lhs.value() * rhs.value()};
}

template<typename LhsDimension, typename RhsDimension, typename Representation, typename LhsScale, typename RhsScale>
constexpr auto operator/(quantity<LhsDimension, Representation, LhsScale> lhs, quantity<RhsDimension, Representation, RhsScale> rhs) -> quantity<dimension_division<LhsDimension, RhsDimension>, Representation, std::ratio_divide<LhsScale, RhsScale>> {
  return quantity<dimension_division<LhsDimension, RhsDimension>, Representation, std::ratio_divide<LhsScale, RhsScale>>{lhs.value() / rhs.value()};
}

template<typename Dimension, typename Representation, typename Scale>
constexpr auto operator*(quantity<Dimension, Representation, Scale> lhs, Representation scale) -> quantity<Dimension, Representation, Scale> {
  return quantity<Dimension, Representation, Scale>{lhs.value() * scale};
}

template<typename Dimension, typename Representation, typename Scale>
constexpr auto operator*(Representation scale, quantity<Dimension, Representation, Scale> lhs) -> quantity<Dimension, Representation, Scale> {
  return quantity<Dimension, Representation, Scale>{scale * lhs.value()};
}

template<typename Dimension, typename Representation, typename Scale>
constexpr auto operator/(quantity<Dimension, Representation, Scale> lhs, Representation scale) -> quantity<Dimension, Representation, Scale> {
  return quantity<Dimension, Representation, Scale>{lhs.value() / scale};
}

template<typename Dimension, typename Representation, typename LhsScale, typename RhsScale>
constexpr auto operator==(quantity<Dimension, Representation, LhsScale> lhs, quantity<Dimension, Representation, RhsScale> rhs) -> bool {
  return lhs.value() == quantity<Dimension, Representation, LhsScale>{rhs}.value();
}

template<typename Dimension, typename Representation, typename LhsScale, typename RhsScale>
constexpr auto operator<=>(quantity<Dimension, Representation, LhsScale> lhs, quantity<Dimension, Representation, RhsScale> rhs) -> std::compare_three_way_result_t<Representation> {
  return lhs.value() <=> quantity<Dimension, Representation, LhsScale>{rhs}.value();
}

using millimeters = quantity<length_dimension, std::float_t, std::milli>;
using centimeters = quantity<length_dimension, std::float_t, std::centi>;
using meters = quantity<length_dimension, std::float_t>;
using kilometers = quantity<length_dimension, std::float_t, std::kilo>;

using milliseconds = quantity<time_dimension, std::float_t, std::milli>;
using seconds = quantity<time_dimension, std::float_t>;
using minutes = quantity<time_dimension, std::float_t, std::ratio<60>>;
using hours = quantity<time_dimension, std::float_t, std::ratio<3600>>;

using grams = quantity<mass_dimension, std::float_t, std::milli>;
using kilograms = quantity<mass_dimension, std::float_t>;

using meters_per_second = quantity<velocity_dimension, std::float_t>;

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

constexpr auto operator""_ms(long double value) -> milliseconds {
  return milliseconds{static_cast<typename milliseconds::representation_type>(value)};
}

constexpr auto operator""_ms(unsigned long long int value) -> milliseconds {
  return milliseconds{static_cast<typename milliseconds::representation_type>(value)};
}

constexpr auto operator""_s(long double value) -> seconds {
  return seconds{static_cast<typename seconds::representation_type>(value)};
}

constexpr auto operator""_s(unsigned long long int value) -> seconds {
  return seconds{static_cast<typename seconds::representation_type>(value)};
}

constexpr auto operator""_kg(long double value) -> kilograms {
  return kilograms{static_cast<typename kilograms::representation_type>(value)};
}

constexpr auto operator""_kg(unsigned long long int value) -> kilograms {
  return kilograms{static_cast<typename kilograms::representation_type>(value)};
}

} // namespace literals

} // namespace sbx::units

template<typename Dimension, typename Representation, typename Scale>
struct fmt::formatter<sbx::units::quantity<Dimension, Representation, Scale>> : fmt::formatter<Representation> {

  template<typename FormatContext>
  auto format(const sbx::units::quantity<Dimension, Representation, Scale>& quantity, FormatContext& context) const -> decltype(context.out()) {
    return fmt::formatter<Representation>::format(quantity.value(), context);
  }

}; // struct fmt::formatter

#endif // LIBSBX_UNITS_QUANTITY_HPP_
