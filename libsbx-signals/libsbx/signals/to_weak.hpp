// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SIGNAL_TO_WEAK_HPP_
#define LIBSBX_SIGNAL_TO_WEAK_HPP_

#include <memory>
#include <type_traits>

namespace sbx::signals {

template<typename Type>
auto to_weak(std::weak_ptr<Type> ptr) -> std::weak_ptr<Type> {
  return ptr;
}

template<typename Type>
auto to_weak(std::shared_ptr<Type> ptr) -> std::weak_ptr<Type> {
  return ptr;
}

template<typename Type, typename = void>
struct is_weak_ptr : std::false_type { };

template<typename Type>
struct is_weak_ptr<Type, std::void_t<decltype(std::declval<Type>().expired()), decltype(std::declval<Type>().lock()), decltype(std::declval<Type>().reset())>> : std::true_type { };

template<typename Type>
constexpr auto is_weak_ptr_v = is_weak_ptr<Type>::value;

template<typename Type, typename = void>
struct is_weak_ptr_compatible : std::false_type { };

template<typename Type>
struct is_weak_ptr_compatible<Type, std::void_t<decltype(to_weak(std::declval<Type>()))>> : is_weak_ptr<decltype(to_weak(std::declval<Type>()))> { };

template<typename Type>
constexpr auto is_weak_ptr_compatible_v = is_weak_ptr_compatible<Type>::value;

} // namespace sbx::signals

#endif // LIBSBX_SIGNAL_TO_WEAK_HPP_
