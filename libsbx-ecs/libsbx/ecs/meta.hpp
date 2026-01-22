// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ECS_META_HPP_
#define LIBSBX_ECS_META_HPP_

#include <type_traits>

#include <libsbx/utility/hashed_string.hpp>

namespace sbx::ecs {

template<typename Type>
struct meta;

template<typename Type, typename = void>
struct has_meta : std::false_type { };

template<typename Type>
struct has_meta<Type, std::void_t<decltype(std::declval<meta<Type>>()(std::declval<const utility::hashed_string&>(), std::declval<Type&>()))>> : std::true_type { };

template<typename Type>
constexpr bool has_meta_v = has_meta<Type>::value;

} // namespace sbx::ecs

#endif // LIBSBX_ECS_META_HPP_
