// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_MATH_TRAITS_HPP_
#define LIBSBX_MATH_TRAITS_HPP_

#include <libsbx/math/constants.hpp>
#include <libsbx/math/concepts.hpp>

namespace sbx::math {

/**
 * @brief Equality comparison for a scalar Type, specialized per Type's numeric category:
 * exact for integral types, epsilon-tolerant for floating-point types.
 *
 * @tparam Type The scalar type to compare as.
 */
template<typename>
struct comparison_traits;

template<integral Type>
struct comparison_traits<Type> {

  /** @return Whether lhs equals rhs, exactly. */
  template<scalar Other>
  inline static constexpr auto equal(Type lhs, Other rhs) noexcept -> bool {
    return lhs == static_cast<Type>(rhs);
  }

}; // struct comparison_traits<integral Type>

template<floating_point Type>
struct comparison_traits<Type> {

  /** @return Whether lhs and rhs differ by at most epsilon_v<Type>. */
  template<scalar Other>
  inline static constexpr auto equal(Type lhs, Other rhs) noexcept -> bool {
    return std::abs(lhs - static_cast<Type>(rhs)) <= epsilon_v<Type>;
  }

}; // struct comparison_traits<floating_point Type>

} // namespace sbx::math

#endif // LIBSBX_MATH_TRAITS_HPP_
