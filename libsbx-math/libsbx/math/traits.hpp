// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MATH_TRAITS_HPP_
#define LIBSBX_MATH_TRAITS_HPP_

#include <libsbx/math/constants.hpp>
#include <libsbx/math/concepts.hpp>

namespace sbx::math {

template<typename>
struct comparision_traits;

template<integral Type>
struct comparision_traits<Type> {

  template<scalar Other>
  inline static constexpr auto equal(Type lhs, Other rhs) noexcept -> bool {
    return lhs == static_cast<Type>(rhs);
  }

}; // template<integral Type>

template<floating_point Type>
struct comparision_traits<Type> {

  template<scalar Other>
  inline static constexpr auto equal(Type lhs, Other rhs) noexcept -> bool {
    return std::abs(lhs - static_cast<Type>(rhs)) <= epsilon_v<Type>;
  }

}; // template<floating_point Type>

} // namespace sbx::math

#endif // LIBSBX_MATH_TRAITS_HPP_
