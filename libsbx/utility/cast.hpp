// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_CAST_HPP_
#define LIBSBX_UTILITY_CAST_HPP_

#include <type_traits>

namespace sbx::utility {

/**
 * @brief Casts a scoped/unscoped enum to its underlying integer type.
 *
 * @tparam Type The enum type.
 *
 * @param value The value to cast.
 *
 * @return value, as std::underlying_type_t<Type>.
 */
template<typename Type>
[[nodiscard]] constexpr auto underlying_cast(Type value) noexcept -> std::underlying_type_t<Type> {
  return static_cast<std::underlying_type_t<Type>>(value);
}

/**
 * @brief Identity cast that only participates in overload resolution via Type, blocking template argument deduction on the parameter — useful to force the caller to specify Type explicitly at the call site.
 *
 * @tparam Type The type to convert to.
 *
 * @param value The value to convert.
 *
 * @return value, converted to Type.
 */
template<typename Type>
[[nodiscard]] constexpr auto implicit_cast(std::type_identity_t<Type> value) noexcept(std::is_nothrow_move_constructible_v<Type>) -> Type {
  return value;
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_CAST_HPP_
