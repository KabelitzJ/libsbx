// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_TIMER_HPP_
#define LIBSBX_UTILITY_TIMER_HPP_

#include <chrono>
#include <cmath>
#include <functional>
#include <type_traits>

#include <libsbx/units/units.hpp>

namespace sbx::utility {

/** @brief A delta timer: elapsed() returns the time since the previous call (or construction) and resets the clock. */
class timer {

public:

  timer();

  ~timer() = default;

  /** @brief Time elapsed since the last call to elapsed() (or since construction, on the first call). */
  auto elapsed() noexcept -> units::seconds;

private:

  std::chrono::time_point<std::chrono::high_resolution_clock> _start{};

}; // class timer

/**
 * @brief RAII scope timer: measures the time until it goes out of scope and passes it
 * to callable on destruction.
 *
 * @tparam Callable A callable invocable with a units::seconds.
 *
 * @param callable Invoked with the elapsed time when the scoped_timer is destroyed.
 */
class scoped_timer {

public:

  template<typename Callable>
  requires (std::is_invocable_v<Callable, const units::seconds&>)
  scoped_timer(Callable&& callable)
  : _on_destroy{std::forward<Callable>(callable)},
    _start{std::chrono::high_resolution_clock::now()} { }

  ~scoped_timer() {
    if (_on_destroy) {
      const auto now = std::chrono::high_resolution_clock::now();
      const auto elapsed = units::seconds{std::chrono::duration_cast<std::chrono::duration<std::float_t>>(now - _start).count()};

      std::invoke(_on_destroy, elapsed);
    }
  }

private:

  std::function<void(const units::seconds&)> _on_destroy;
  std::chrono::time_point<std::chrono::high_resolution_clock> _start;

}; // class scoped_timer

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_TIMER_HPP_
