// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_TYPE_LIST_HPP_
#define LIBSBX_UTILITY_TYPE_LIST_HPP_

#include <cstddef>

namespace sbx::utility {

template<typename... Type>
struct type_list {
  using type = type_list;
  inline static constexpr auto size = sizeof...(Type);
}; // struct type_list

template<std::size_t, typename>
struct type_list_element;

template<std::size_t Index, typename First, typename... Other>
struct type_list_element<Index, type_list<First, Other...>> : type_list_element<Index - 1u, type_list<Other...>> { };

template<typename First, typename... Other>
struct type_list_element<0u, type_list<First, Other...>> {
  using type = First;
}; // struct type_list_element

template<std::size_t Index, typename List>
using type_list_element_t = typename type_list_element<Index, List>::type;

template<typename, typename>
struct type_list_index;

template<typename Type, typename First, typename... Other>
struct type_list_index<Type, type_list<First, Other...>> {
  using value_type = std::size_t;
  inline static constexpr auto value = 1u + type_list_index<Type, type_list<Other...>>::value;
}; // struct type_list_index

template<typename Type, typename... Other>
requires (type_list_index<Type, type_list<Other...>>::value == sizeof...(Other))
struct type_list_index<Type, type_list<Type, Other...>> {
  using value_type = std::size_t;
  inline static constexpr auto value = 0u;
}; // struct type_list_index

template<typename Type>
struct type_list_index<Type, type_list<>> {
  using value_type = std::size_t;
  inline static constexpr auto value = 0u;
}; // struct type_list_index

template<typename Type, typename List>
inline constexpr auto type_list_index_v = type_list_index<Type, List>::value;

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_TYPE_LIST_HPP_
