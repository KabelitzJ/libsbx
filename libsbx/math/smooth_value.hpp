// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_SMOOTH_VALUE_HPP_
#define LIBSBX_MATH_SMOOTH_VALUE_HPP_

#include <cmath>
#include <concepts>
#include <type_traits>

#include <libsbx/units/time.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/traits.hpp>
#include <libsbx/math/algorithm.hpp>

namespace sbx::math {

enum class smoothing_mode : std::uint8_t {
  linear,
  proportional
}; // enum class smoothing_mode

template<typename Type>
struct is_smoothable : std::false_type { };

template<typename Type>
inline constexpr auto is_smoothable_v = is_smoothable<Type>::value;

template<floating_point Type>
struct is_smoothable<Type> : std::true_type { };

template<typename Type>
concept smoothable = is_smoothable_v<Type>;

template<smoothable Type, smoothing_mode Mode>
class basic_smooth_value {

public:

  using value_type = Type;

  inline static constexpr auto mode = Mode;

  basic_smooth_value(const value_type value)
  : _current{value},
    _target{value} { }

  static constexpr auto clamp(const basic_smooth_value& value, const value_type min, const value_type max) -> basic_smooth_value {
    return basic_smooth_value{value._current, std::clamp(value._target, min, max)};
  }

  template<floating_point Ratio>
  static constexpr auto lerp(const basic_smooth_value& x, const basic_smooth_value& y, const Ratio ratio) -> basic_smooth_value {
    return basic_smooth_value{x._current, mix(x._target, y._target, ratio)};
  }

  constexpr auto operator=(const value_type value) -> basic_smooth_value& {
    _target = value;

    return *this;
  }

  constexpr auto operator+=(const value_type value) -> basic_smooth_value& {
    _target += value;

    return *this;
  }

  constexpr auto operator-=(const value_type value) -> basic_smooth_value& {
    _target -= value;

    return *this;
  }

  constexpr auto operator*=(const value_type value) -> basic_smooth_value& {
    _target *= value;

    return *this;
  }

  constexpr auto operator/=(const value_type value) -> basic_smooth_value& {
    _target /= value;

    return *this;
  }

  constexpr auto operator+(const value_type value) -> basic_smooth_value {
    auto copy = basic_smooth_value{*this};
    copy += value;
    return copy;
  }

  constexpr auto operator-(const value_type value) -> basic_smooth_value {
    auto copy = basic_smooth_value{*this};
    copy -= value;
    return copy;
  }

  constexpr auto operator*(const value_type value) -> basic_smooth_value {
    auto copy = basic_smooth_value{*this};
    copy *= value;
    return copy;
  }

  constexpr auto operator/(const value_type value) -> basic_smooth_value {
    auto copy = basic_smooth_value{*this};
    copy /= value;
    return copy;
  }

  constexpr auto value() const noexcept -> value_type {
    return _current;
  }

  constexpr operator value_type() const noexcept {
    return value();
  }

  constexpr void update(const units::second& delta_time, const value_type base_speed) {
    const auto difference = _target - _current;

    if (comparision_traits<value_type>::equal(difference, static_cast<value_type>(0))) {
      _current = _target;
      return;
    }

    const auto step = _compute_step(difference, base_speed, delta_time);

    // Clamp step to not overshoot
    if (std::abs(step) >= std::abs(difference)) {
      _current = _target;
    } else {
      _current += step;
    }
  }


private:

  basic_smooth_value(const value_type current, const value_type target)
  : _current{current},
    _target{target} { }

  constexpr auto _compute_step(const value_type difference, const value_type base_speed, const units::second& delta_time) const -> value_type {
    switch (mode) {
      case smoothing_mode::linear: {
        return (difference > 0 ? 1 : -1) * base_speed * delta_time;
      }
      case smoothing_mode::proportional: {
        return difference * base_speed * delta_time;
      }
      default: {
        return {};
      }
    }
  }

  value_type _current;
  value_type _target;

}; // class basic_smooth_value

template<smoothable Type>
using basic_linear_smooth_value = basic_smooth_value<Type, smoothing_mode::linear>;

using linear_smooth_valuef = basic_linear_smooth_value<std::float_t>;

using linear_smooth_value = linear_smooth_valuef;

template<smoothable Type>
using basic_proportional_smooth_value = basic_smooth_value<Type, smoothing_mode::proportional>;

using proportional_smooth_valuef = basic_proportional_smooth_value<std::float_t>;

using proportional_smooth_value = proportional_smooth_valuef;

} // namespace sbx::math

#endif // LIBSBX_MATH_SMOOTH_VALUE_HPP_