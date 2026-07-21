#ifndef LIBSBX_UNITS_TIME_HPP_
#define LIBSBX_UNITS_TIME_HPP_

#include <ratio>

#include <libsbx/units/quantity.hpp>

namespace sbx::units {

using milliseconds = quantity<time_dimension, std::float_t, std::milli>;
using seconds = quantity<time_dimension, std::float_t>;
using minutes = quantity<time_dimension, std::float_t, std::ratio<60>>;
using hours = quantity<time_dimension, std::float_t, std::ratio<3600>>;

template<>
struct unit_formatter<time_dimension, std::milli> {
  static constexpr auto symbol = "ms";
}; // struct unit_formatter

template<>
struct unit_formatter<time_dimension, std::ratio<1>> {
  static constexpr auto symbol = "s";
}; // struct unit_formatter

template<>
struct unit_formatter<time_dimension, std::ratio<60>> {
  static constexpr auto symbol = "min";
}; // struct unit_formatter

template<>
struct unit_formatter<time_dimension, std::ratio<3600>> {
  static constexpr auto symbol = "h";
}; // struct unit_formatter

namespace literals {

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

constexpr auto operator""_min(long double value) -> minutes {
  return minutes{static_cast<typename minutes::representation_type>(value)};
}

constexpr auto operator""_min(unsigned long long int value) -> minutes {
  return minutes{static_cast<typename minutes::representation_type>(value)};
}

constexpr auto operator""_h(long double value) -> hours {
  return hours{static_cast<typename hours::representation_type>(value)};
}

constexpr auto operator""_h(unsigned long long int value) -> hours {
  return hours{static_cast<typename hours::representation_type>(value)};
}

} // namespace literals

} // namespace sbx::units

#endif // LIBSBX_UNITS_TIME_HPP_
