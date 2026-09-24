// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_CONCEPTS_HPP_
#define LIBSBX_UTILITY_CONCEPTS_HPP_

#include <type_traits>

namespace sbx::utility {

/**
 * @brief Always false, regardless of Type — for a static_assert in an `if constexpr` branch that must never be instantiated (a plain `static_assert(false)` would fire even when the branch is discarded).
 *
 * @tparam Type Any type; never actually inspected.
 */
template<typename Type>
struct is_always_false : std::false_type { };

template<typename Type>
inline constexpr auto is_always_false_v = is_always_false<Type>::value;

/** @brief See is_always_false. */
template<typename Type>
concept always_false = is_always_false_v<Type>;

template<typename Type, typename... TypeList>
struct is_one_of : std::false_type { };

template<typename Type, typename... TypeList>
struct is_one_of<Type, Type, TypeList...> : std::true_type { };

template<typename Type, typename Head, typename... Rest>
struct is_one_of<Type, Head, Rest...> : is_one_of<Type, Rest...> { };

/**
 * @brief Whether Type is exactly one of TypeList.
 *
 * @tparam Type The type to look for.
 * @tparam TypeList The candidate types.
 */
template<typename Type, typename... TypeList>
inline constexpr auto is_one_of_v = is_one_of<Type, TypeList...>::value;

/** @brief See is_one_of. */
template<typename Type, typename... TypeList>
concept one_of = is_one_of_v<Type, TypeList...>;

/** @brief The negation of one_of. */
template<typename Type, typename... TypeList>
concept none_of = !is_one_of_v<Type, TypeList...>;

template<typename... Types>
struct are_unique : std::true_type { };

template<typename First, typename... Rest>
struct are_unique<First, Rest...> : std::bool_constant<(!std::is_same_v<First, Rest> && ...) && are_unique<Rest...>::value> { };

/**
 * @brief Whether every type in Types is distinct.
 *
 * @tparam Types The types to check.
 */
template<typename... Types>
inline constexpr auto are_unique_v = are_unique<Types...>::value;

/** @brief See are_unique. */
template<typename... Types>
concept unique = are_unique_v<Types...>;

/**
 * @brief Whether Type appears in Types.
 *
 * @tparam Type The type to look for.
 * @tparam Types The candidate types.
 */
template<typename Type, typename... Types>
inline constexpr auto contains_v = (std::is_same_v<Type, Types> || ...);

template<typename Type, typename... TypeList>
struct is_convertible_to_one_of : std::false_type{ };

template<typename Type, typename... TypeList>
struct is_convertible_to_one_of<Type, Type, TypeList...> : std::true_type { };

template<typename Type, typename Head, typename... Rest>
struct is_convertible_to_one_of<Type, Head, Rest...> : std::conditional_t<std::is_convertible_v<Type, Head>, std::true_type, is_convertible_to_one_of<Type, Rest...>> { };

/**
 * @brief Whether Type is convertible to at least one of TypeList.
 *
 * @tparam Type The type to check.
 * @tparam TypeList The candidate target types.
 */
template<typename Type, typename... TypeList>
inline constexpr auto is_convertible_to_one_of_v = is_convertible_to_one_of<Type, TypeList...>::value;

/** @brief See is_convertible_to_one_of. */
template<typename Type, typename... TypeList>
concept convertible_to_one_of = is_convertible_to_one_of_v<Type, TypeList...>;


/**
 * @brief Whether Type is a concrete (non-abstract) class that publicly derives from
 * Base, where Base has a virtual destructor.
 *
 * @tparam Type The candidate derived type.
 * @tparam Base The polymorphic base type.
 */
template<typename Type, typename Base>
concept implements = !std::is_abstract_v<Type> && std::has_virtual_destructor_v<Base> && std::is_base_of_v<Base, Type>;

/** @brief Whether Type is a complete (fully defined, non-void) type at the point of use. */
template<typename Type>
struct is_complete : std::bool_constant<(sizeof(Type) != 0 && !std::is_void_v<Type>)> { };

template<typename Type>
inline constexpr auto is_complete_v = is_complete<Type>::value;

/** @brief See is_complete. */
template<typename Type>
concept complete = is_complete_v<Type>;

/**
 * @brief To, with From's const-qualification applied.
 *
 * @tparam To The type to apply const-qualification to.
 * @tparam From The type whose const-qualification is copied.
 */
template<typename To, typename From>
struct constness_as {
  using type = std::remove_const_t<To>;
}; // struct constness_as

template<typename To, typename From>
struct constness_as<To, const From> {
  using type = const To;
}; // struct constness_as

/** @brief See constness_as. */
template<typename To, typename From>
using constness_as_t = typename constness_as<To, From>::type;

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_CONCEPTS_HPP_
