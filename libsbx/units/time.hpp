// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UNITS_TIME_HPP_
#define LIBSBX_UNITS_TIME_HPP_

#include <libsbx/units/quantity.hpp>

namespace sbx::units {

namespace detail {

struct time_tag { };

} // namespace detail

using year = quantity<detail::time_tag, std::float_t, std::ratio<31556952>>;
using month = quantity<detail::time_tag, std::float_t, std::ratio<2629746>>;
using week = quantity<detail::time_tag, std::float_t, std::ratio<604800>>;
using day = quantity<detail::time_tag, std::float_t, std::ratio<86400>>;
using hour = quantity<detail::time_tag, std::float_t, std::ratio<3600>>;
using minute = quantity<detail::time_tag, std::float_t, std::ratio<60>>;
using second = quantity<detail::time_tag, std::float_t>;
using millisecond = quantity<detail::time_tag, std::float_t, std::milli>;
using microsecond = quantity<detail::time_tag, std::float_t, std::micro>;
using nanosecond = quantity<detail::time_tag, std::float_t, std::nano>;

namespace literals {

constexpr auto operator""_y(long double value) -> year {
  return year{static_cast<year::value_type>(value)};
}

constexpr auto operator""_y(unsigned long long value) -> year {
  return year{static_cast<year::value_type>(value)};
}

constexpr auto operator""_mo(long double value) -> month {
  return month{static_cast<month::value_type>(value)};
}

constexpr auto operator""_mo(unsigned long long value) -> month {
  return month{static_cast<month::value_type>(value)};
}

constexpr auto operator""_w(long double value) -> week {
  return week{static_cast<week::value_type>(value)};
}

constexpr auto operator""_w(unsigned long long value) -> week {
  return week{static_cast<week::value_type>(value)};
}

constexpr auto operator""_d(long double value) -> day {
  return day{static_cast<day::value_type>(value)};
}

constexpr auto operator""_d(unsigned long long value) -> day {
  return day{static_cast<day::value_type>(value)};
}

constexpr auto operator""_h(long double value) -> hour {
  return hour{static_cast<hour::value_type>(value)};
}

constexpr auto operator""_h(unsigned long long value) -> hour {
  return hour{static_cast<hour::value_type>(value)};
}

constexpr auto operator""_min(long double value) -> minute {
  return minute{static_cast<minute::value_type>(value)};
}

constexpr auto operator""_min(unsigned long long value) -> minute {
  return minute{static_cast<minute::value_type>(value)};
}

constexpr auto operator""_s(long double value) -> second {
  return second{static_cast<second::value_type>(value)};
}

constexpr auto operator""_s(unsigned long long value) -> second {
  return second{static_cast<second::value_type>(value)};
}

constexpr auto operator""_ms(long double value) -> millisecond {
  return millisecond{static_cast<millisecond::value_type>(value)};
}

constexpr auto operator""_ms(unsigned long long value) -> millisecond {
  return millisecond{static_cast<millisecond::value_type>(value)};
}

constexpr auto operator""_us(long double value) -> microsecond {
  return microsecond{static_cast<microsecond::value_type>(value)};
}

constexpr auto operator""_us(unsigned long long value) -> microsecond {
  return microsecond{static_cast<microsecond::value_type>(value)};
}

constexpr auto operator""_ns(long double value) -> nanosecond {
  return nanosecond{static_cast<nanosecond::value_type>(value)};
}

constexpr auto operator""_ns(unsigned long long value) -> nanosecond {
  return nanosecond{static_cast<nanosecond::value_type>(value)};
}

};

} // namespace sbx::units

#endif // LIBSBX_UNITS_TIME_HPP_
