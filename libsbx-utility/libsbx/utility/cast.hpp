// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_CAST_HPP_
#define LIBSBX_UTILITY_CAST_HPP_

#include <type_traits>

namespace sbx::utility {

template<typename Type>
[[nodiscard]] constexpr auto underlying_cast(Type value) noexcept -> std::underlying_type_t<Type> {
  return static_cast<std::underlying_type_t<Type>>(value);
}

template<typename Type>
[[nodiscard]] constexpr auto implicit_cast(std::type_identity_t<Type> value) noexcept(std::is_nothrow_move_constructible_v<Type>) -> Type {
  return value;
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_CAST_HPP_
