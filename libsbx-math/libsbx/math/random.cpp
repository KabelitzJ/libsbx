// SPDX-License-Identifier: MIT
#include <libsbx/math/random.hpp>

namespace sbx::math {

auto random_color(const std::float_t alpha) -> color {
  return color{random::next<std::float_t>(0.0f, 1.0f), random::next<std::float_t>(0.0f, 1.0f), random::next<std::float_t>(0.0f, 1.0f), alpha};
}

} // namespace sbx::math
