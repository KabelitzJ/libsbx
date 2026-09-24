// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/math/algorithm.hpp>

namespace sbx::math {

template<floating_point Type>
inline constexpr auto mix(const Type x, const Type y, const Type a) -> Type {
  return x * (static_cast<Type>(1) - a) + y * a;
}

template<floating_point Type>
inline constexpr auto abs(const Type value) -> Type {
  return std::abs(value);
}

template<floating_point Type>
inline constexpr auto sqrt(const Type value) -> Type {
  if (value < Type{0}) {
    return std::numeric_limits<Type>::quiet_NaN();
  }

  if (value == Type{0} || value == std::numeric_limits<Type>::infinity()) {
    return value;
  }

  auto result = value;

  for (auto iteration = 0; iteration < 64; ++iteration) {
    const auto next = Type{0.5} * (result + value / result);

    if (next == result) {
      break;
    }

    result = next;
  }

  return result;
}

} // namespace sbx::math
