// SPDX-License-Identifier: MIT
#ifndef DEMO_SIMULATION_TICK_CLOCK_HPP_
#define DEMO_SIMULATION_TICK_CLOCK_HPP_

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace demo {

enum class simulation_speed : std::uint8_t {
  paused = 0,
  normal = 1,
  fast = 2,
  faster = 4,
}; // enum class simulation_speed

struct tick_time {
  std::uint64_t tick{0};
  std::uint32_t hour{0};
  std::uint32_t day{1};
  std::uint32_t month{1};
  std::uint32_t year{1};
}; // struct tick_time

class tick_clock {

public:

  // One in-game hour per tick, 24 ticks per day.
  // At 1x speed, one tick fires every `seconds_per_tick` real seconds.
  static constexpr auto ticks_per_day = 24u;
  static constexpr auto days_per_month = 30u;
  static constexpr auto months_per_year = 12u;
  static constexpr auto seconds_per_tick = 1.0f;
  static constexpr auto max_ticks_per_frame = 8u;

  tick_clock() = default;

  // Call once per frame with the real delta time in seconds.
  // Returns the number of ticks that should fire this frame (0 if paused or accumulator not full).
  auto advance(std::float_t real_delta_seconds) -> std::uint32_t {
    if (_speed == simulation_speed::paused) {
      return 0;
    }

    auto speed_multiplier = static_cast<std::float_t>(static_cast<std::uint8_t>(_speed));

    _accumulator += real_delta_seconds * speed_multiplier;

    auto ticks_this_frame = static_cast<std::uint32_t>(_accumulator / seconds_per_tick);
    ticks_this_frame = std::min(ticks_this_frame, max_ticks_per_frame);

    _accumulator -= static_cast<std::float_t>(ticks_this_frame) * seconds_per_tick;

    // Prevent accumulator from growing unbounded during lag spikes
    _accumulator = std::min(_accumulator, seconds_per_tick * 2.0f);

    for (auto i = 0u; i < ticks_this_frame; ++i) {
      _advance_tick();
    }

    return ticks_this_frame;
  }

  auto set_speed(simulation_speed speed) -> void {
    _speed = speed;
  }

  auto speed() const -> simulation_speed {
    return _speed;
  }

  auto toggle_pause() -> void {
    if (_speed == simulation_speed::paused) {
      _speed = _speed_before_pause;
    } else {
      _speed_before_pause = _speed;
      _speed = simulation_speed::paused;
    }
  }

  auto is_paused() const -> bool {
    return _speed == simulation_speed::paused;
  }

  // Smooth time-of-day for rendering (0.0 = midnight, 12.0 = noon, 23.99.. = just before midnight).
  // Interpolates between discrete ticks using the accumulator fraction.
  auto continuous_hour() const -> std::float_t {
    auto fraction = _accumulator / seconds_per_tick;

    return static_cast<std::float_t>(_time.hour) + std::clamp(fraction, 0.0f, 1.0f);
  }

  auto time() const -> const tick_time& {
    return _time;
  }

  auto total_ticks() const -> std::uint64_t {
    return _time.tick;
  }

  auto speed_name() const -> const char* {
    switch (_speed) {
      case simulation_speed::paused: return "paused";
      case simulation_speed::normal: return "1x";
      case simulation_speed::fast:   return "2x";
      case simulation_speed::faster: return "4x";
      default:                       return "?";
    }
  }

private:

  auto _advance_tick() -> void {
    ++_time.tick;
    ++_time.hour;

    if (_time.hour >= ticks_per_day) {
      _time.hour = 0;
      ++_time.day;

      if (_time.day > days_per_month) {
        _time.day = 1;
        ++_time.month;

        if (_time.month > months_per_year) {
          _time.month = 1;
          ++_time.year;
        }
      }
    }
  }

  simulation_speed _speed{simulation_speed::normal};
  simulation_speed _speed_before_pause{simulation_speed::normal};
  std::float_t _accumulator{0.0f};
  tick_time _time;

}; // class tick_clock

} // namespace demo

#endif // DEMO_SIMULATION_TICK_CLOCK_HPP_
