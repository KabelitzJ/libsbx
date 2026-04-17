// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_MAKE_ARRAY_HPP_
#define LIBSBX_UTILITY_MAKE_ARRAY_HPP_

#include <array>
#include <utility>
#include <concepts>
#include <functional>

namespace sbx::utility {

namespace detail {

template<bool Condition, typename Type, std::convertible_to<Type> Other = Type>
constexpr auto conditional_value(Other value) noexcept -> Type {
  if constexpr (Condition) {
    return static_cast<Type>(value);
  } else {
    return Type{0};
  }
}

template<typename Type, std::size_t... Indices, std::convertible_to<Type>... Args>
requires (sizeof...(Args) == sizeof...(Indices))
constexpr auto make_array_impl(std::index_sequence<Indices...>, Args&&... args) -> std::array<Type, sizeof...(Indices)> {
  return std::array<Type, sizeof...(Indices)>{ static_cast<Type>(static_cast<void>(Indices), std::forward<Args>(args))... };
}

template<typename Type, std::size_t... Indices, std::convertible_to<Type> Other = Type>
constexpr auto make_array_impl(std::index_sequence<Indices...>, const Other& value) -> std::array<Type, sizeof...(Indices)> {
  return std::array<Type, sizeof...(Indices)>{ static_cast<Type>(static_cast<void>(Indices), value)... };
}

template<std::size_t Index, typename Type, std::size_t... Indices, std::convertible_to<Type> Other = Type>
requires (Index >= 0 && Index < sizeof...(Indices))
constexpr auto make_array_impl(std::index_sequence<Indices...>, const Other& value) -> std::array<Type, sizeof...(Indices)> {
  return std::array<Type, sizeof...(Indices)>{ conditional_value<(Indices == Index), Type>(value)... };
}

template<typename Type, std::size_t... Indices, std::convertible_to<Type> Other = Type>
constexpr auto make_array_impl(std::index_sequence<Indices...>, const std::array<Other, sizeof...(Indices)>& array) -> std::array<Type, sizeof...(Indices)> {
  return std::array<Type, sizeof...(Indices)>{ static_cast<Type>(array[Indices])... };
}

} // namespace detail

template<typename Type, std::size_t Size, std::convertible_to<Type>... Args>
requires (sizeof...(Args) == Size)
constexpr auto make_array(Args&&... args) -> std::array<Type, Size> {
  return detail::make_array_impl<Type>(std::make_index_sequence<Size>(), std::forward<Args>(args)...);
}

template<typename Type, std::size_t Size, std::convertible_to<Type> Other = Type>
constexpr auto make_array(const Other& value) -> std::array<Type, Size> {
  return detail::make_array_impl<Type>(std::make_index_sequence<Size>(), value);
}

template<typename Type, std::size_t Size, std::size_t Index, std::convertible_to<Type> Other = Type>
constexpr auto make_array(const Other& value) -> std::array<Type, Size> {
  return detail::make_array_impl<Index, Type>(std::make_index_sequence<Size>(), value);
}

template<typename Type, std::size_t Size, std::convertible_to<Type> Other = Type>
constexpr auto make_array(const std::array<Other, Size>& array) -> std::array<Type, Size> {
  return detail::make_array_impl<Type>(std::make_index_sequence<Size>(), array);
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_MAKE_ARRAY_HPP_
