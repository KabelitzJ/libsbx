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

class timer {

public:

  timer();

  ~timer() = default;

  auto elapsed() noexcept -> units::seconds;

private:

  std::chrono::time_point<std::chrono::high_resolution_clock> _start{};

}; // class timer

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
