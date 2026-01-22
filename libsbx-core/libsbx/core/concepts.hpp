// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CORE_CONCEPTS_HPP_
#define LIBSBX_CORE_CONCEPTS_HPP_

#include <type_traits>
#include <concepts>

namespace sbx::core {

/**
 * @brief  Describes a type or object that can be invoked with the give parameters and return the given type
 * 
 * @tparam Callable Type of the callable
 * @tparam Return Return type of the callable
 * @tparam Args... Types of the arguments of the callable
 */
template<typename Callable, typename Return, typename... Args>
concept callable = std::is_invocable_r_v<Return, Callable, Args...>;

template<typename Iterable>
concept iterable = requires(Iterable iterable) {
  { Iterable::iterator };
  { iterable.begin() } -> std::same_as<typename Iterable::iterator>;
  { iterable.end() } -> std::same_as<typename Iterable::iterator>;
}; // concept iterable

template<typename Iterable>
concept const_iterable = requires(Iterable iterable) {
  { Iterable::const_iterator };
  { iterable.cbegin() } -> std::same_as<typename Iterable::const_iterator>;
  { iterable.cend() } -> std::same_as<typename Iterable::const_iterator>;
}; // concept const_iterable

template<typename Iterable>
concept reverse_iterable = requires(Iterable iterable) {
  { Iterable::reverse_iterator };
  { iterable.rbegin() } -> std::same_as<typename Iterable::reverse_iterator>;
  { iterable.rend() } -> std::same_as<typename Iterable::reverse_iterator>;
}; // concept reverse_iterable

} // namespace sbx::core

#endif // LIBSBX_CORE_CONCEPTS_HPP_
