// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_FAST_MOD_HPP_
#define LIBSBX_UTILITY_FAST_MOD_HPP_

#include <concepts>
#include <cmath>

namespace sbx::utility {

/**
 * @brief Fast modulus operation. If the value is less than the modulus, the value can be returned directly.
 * 
 * @tparam Value The type of the value.
 * @tparam Mod The type of the modulus.
 * 
 * @param value The value to be modded.
 * @param modulus The modulus to be used.
 * 
 * @return Type The result of the modulus operation.
 */
template<std::unsigned_integral Value, std::unsigned_integral Mod>
constexpr auto fast_mod(const Value value, const Mod modulus) noexcept -> Value {
  // return value - (value / modulus) * modulus;
  return value < modulus ? value : value % static_cast<Value>(modulus);
}

template<std::floating_point Type>
constexpr auto fast_mod(const Type value, const Type modulus) noexcept -> Type {
  // return value - (value / modulus) * modulus;
  return value < modulus ? value : std::fmod(value, modulus);
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_FAST_MOD_HPP_
