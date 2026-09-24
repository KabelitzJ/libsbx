// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_OVERLOAD_HPP_
#define LIBSBX_UTILITY_OVERLOAD_HPP_

#include <type_traits>
#include <utility>

namespace sbx::utility {

namespace detail {

template<typename... Callables>
struct overloader;

template<>
struct overloader<> {};

template<typename Callable>
struct overloader<Callable> : Callable {

  using Callable::operator();

  template<typename Other>
  constexpr overloader(Other&& other)
  : Callable{std::forward<Other>(other)} { }

}; // struct overloader

template<typename Callable, typename... Callables>
struct overloader<Callable, Callables...> : Callable, overloader<Callables...> {

  using Callable::operator();
  using overloader<Callables...>::operator();

  template<typename Other, typename... Others>
  constexpr overloader(Other&& other, Others&&... others)
  : Callable{std::forward<Other>(other)}, overloader<Callables...>{std::forward<Others>(others)...} { }

}; // struct overloader

} // namespace detail

/**
 * @brief Combines several callables (e.g. lambdas) into a single callable with all of their operator()s overloaded — typically used as a visitor for std::visit.
 *
 * @tparam Callables The types of the callables to combine.
 *
 * @param callables The callables to combine.
 *
 * @return A callable that overload-resolves between all of callables' operator()s.
 */
template<typename... Callables>
[[nodiscard]] constexpr auto overload(Callables&&... callables) noexcept(noexcept(detail::overloader<std::decay_t<Callables>...>{std::forward<Callables>(callables)...})) {
  return detail::overloader<std::decay_t<Callables>...>{std::forward<Callables>(callables)...};
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_OVERLOAD_HPP_
