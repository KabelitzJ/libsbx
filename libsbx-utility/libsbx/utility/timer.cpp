// SPDX-License-Identifier: MIT
#include <libsbx/utility/timer.hpp>

namespace sbx::utility {

timer::timer()
: _start{std::chrono::high_resolution_clock::now()} { }

auto timer::elapsed() noexcept -> units::second {
  const auto now = std::chrono::high_resolution_clock::now();
  const auto elapsed = units::second{std::chrono::duration_cast<std::chrono::duration<std::float_t>>(now - _start).count()};

  _start = now;

  return elapsed;
}

} // namespace sbx::utility
