// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/utility/timer.hpp>

namespace sbx::utility {

timer::timer()
: _start{std::chrono::high_resolution_clock::now()} { }

auto timer::elapsed() noexcept -> units::seconds {
  const auto now = std::chrono::high_resolution_clock::now();
  const auto elapsed = units::seconds{std::chrono::duration_cast<std::chrono::duration<std::float_t>>(now - _start).count()};

  _start = now;

  return elapsed;
}

} // namespace sbx::utility
