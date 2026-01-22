// SPDX-License-Identifier: MIT
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
  if (std::signbit(value)) {
    return std::numeric_limits<Type>::quiet_NaN();
  }

  if (value == std::numeric_limits<Type>::infinity()) {
    return std::numeric_limits<Type>::quiet_NaN();
  }

  auto result = Type{value};

  for (auto last = Type{0.0}; result != last; result = Type{0.5} * (result + value / result)) {
    last = result;
  }

  return result;
}

} // namespace sbx::math
