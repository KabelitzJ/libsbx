// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MATH_SMOOTH_VALUE_HPP_
#define LIBSBX_MATH_SMOOTH_VALUE_HPP_

#include <algorithm>
#include <cmath>
#include <concepts>
#include <type_traits>

#include <libsbx/units/units.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/concepts.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/traits.hpp>
#include <libsbx/math/algorithm.hpp>

namespace sbx::math {

/** @brief How basic_smooth_value moves its current value toward its target on each update(). */
enum class smoothing_mode : std::uint8_t {
  /** @brief Moves at a constant rate (base_speed units per second), reaching the target in finite time. */
  linear,
  /** @brief Moves at a rate proportional to the remaining distance, approaching the target asymptotically. */
  proportional
}; // enum class smoothing_mode

template<typename Type>
struct is_smoothable : std::false_type { };

template<typename Type>
inline constexpr auto is_smoothable_v = is_smoothable<Type>::value;

template<floating_point Type>
struct is_smoothable<Type> : std::true_type { };

/** @brief A type basic_smooth_value can smooth — currently any floating-point type. */
template<typename Type>
concept smoothable = is_smoothable_v<Type>;

/**
 * @brief A value that, instead of jumping straight to a newly assigned target, moves toward it gradually over successive update() calls — e.g. for a camera field-of-view or a UI element animating toward a new value instead of snapping to it.
 *
 * @tparam Type The underlying value type; must satisfy smoothable.
 * @tparam Mode How the value approaches its target; see smoothing_mode.
 */
template<smoothable Type, smoothing_mode Mode>
class basic_smooth_value {

public:

  using value_type = Type;

  inline static constexpr auto mode = Mode;

  /** @brief Constructs already at value, with no smoothing in progress (current == target). */
  basic_smooth_value(const value_type value)
  : _current{value},
    _target{value} { }

  /** @brief Clamps value's target into [min, max], leaving its current (in-progress) value untouched. */
  static constexpr auto clamp(const basic_smooth_value& value, const value_type min, const value_type max) -> basic_smooth_value {
    return basic_smooth_value{value._current, std::clamp(value._target, min, max)};
  }

  /**
   * @brief Interpolates between two smooth values' targets, keeping x's current (in-progress) value.
   *
   * @tparam Ratio The interpolation factor's type.
   *
   * @param x The value whose current (in-progress) value carries over.
   * @param y The value to interpolate toward.
   * @param ratio The interpolation factor, in [0, 1].
   *
   * @return A basic_smooth_value with x's current value and a target interpolated between x's and y's targets.
   */
  template<floating_point Ratio>
  static constexpr auto lerp(const basic_smooth_value& x, const basic_smooth_value& y, const Ratio ratio) -> basic_smooth_value {
    return basic_smooth_value{x._current, mix(x._target, y._target, ratio)};
  }

  /** @brief Sets a new target to smooth toward. */
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

  constexpr auto operator+(const value_type value) const -> basic_smooth_value {
    auto copy = basic_smooth_value{*this};
    copy += value;
    return copy;
  }

  constexpr auto operator-(const value_type value) const -> basic_smooth_value {
    auto copy = basic_smooth_value{*this};
    copy -= value;
    return copy;
  }

  constexpr auto operator*(const value_type value) const -> basic_smooth_value {
    auto copy = basic_smooth_value{*this};
    copy *= value;
    return copy;
  }

  constexpr auto operator/(const value_type value) const -> basic_smooth_value {
    auto copy = basic_smooth_value{*this};
    copy /= value;
    return copy;
  }

  /** @return The current (in-progress) value — not the target. */
  constexpr auto value() const noexcept -> value_type {
    return _current;
  }

  constexpr operator value_type() const noexcept {
    return value();
  }

  /**
   * @brief Advances the current value toward the target by one step, sized according to mode, base_speed and delta_time. Snaps exactly to the target instead of overshooting it once the remaining step would be at least as large as the remaining distance.
   *
   * @param delta_time The elapsed time since the last update() call.
   * @param base_speed The smoothing rate; its meaning depends on mode (see smoothing_mode).
   */
  constexpr void update(const units::seconds& delta_time, const value_type base_speed) {
    const auto difference = _target - _current;

    if (comparison_traits<value_type>::equal(difference, static_cast<value_type>(0))) {
      _current = _target;
      return;
    }

    const auto step = _compute_step(difference, base_speed, delta_time);

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

  constexpr auto _compute_step(const value_type difference, const value_type base_speed, const units::seconds& delta_time) const -> value_type {
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
