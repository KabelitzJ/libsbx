// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ECS_COMPONENT_HPP_
#define LIBSBX_ECS_COMPONENT_HPP_

#include <type_traits>

namespace sbx::ecs {

namespace detail {

template<typename Type, typename = void>
struct in_place_delete: std::bool_constant<!(std::is_move_constructible_v<Type> && std::is_move_assignable_v<Type>)> { };

template<>
struct in_place_delete<void>: std::false_type { };

template<typename Type>
struct in_place_delete<Type, std::enable_if_t<Type::in_place_delete>> : std::true_type {  };

template<typename Type, typename = void>
struct page_size: std::integral_constant<std::size_t, !std::is_empty_v<Type> * 1024u> { };

template<>
struct page_size<void>: std::integral_constant<std::size_t, 0u> { };

template<typename Type>
struct page_size<Type, std::void_t<decltype(Type::page_size)>> : std::integral_constant<std::size_t, Type::page_size> { };

} // namespace detail

template<typename Type, typename Entity>
requires (std::is_same_v<std::decay_t<Type>, Type>)
struct component_traits {

  using element_type = Type;
  using entity_type = Entity;

  static constexpr bool in_place_delete = detail::in_place_delete<Type>::value;
  static constexpr std::size_t page_size = detail::page_size<Type>::value;

}; // struct component_traits

} // namespace sbx::ecs

#endif // LIBSBX_ECS_COMPONENT_HPP_
