// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_FIELD_DIFF_HPP_
#define EDITOR_COMMANDS_FIELD_DIFF_HPP_

#include <concepts>
#include <cstddef>
#include <meta>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

#include <libsbx/math/vector.hpp>

namespace editor {

namespace detail {

// Declaration only: deduces a basic_vector base's size in unevaluated contexts.
template<std::size_t Size, typename Type>
auto math_vector_size(const sbx::math::basic_vector<Size, Type>&) -> std::integral_constant<std::size_t, Size>;

template<typename Type>
concept math_vector = requires(const Type& value) { detail::math_vector_size(value); };

template<typename Type>
inline constexpr auto is_variant_v = false;

template<typename... Types>
inline constexpr auto is_variant_v<std::variant<Types...>> = true;

// std::equality_comparable isn't enough: libstdc++'s == for vector/pair/optional/variant is unconstrained and only fails on instantiation (e.g. animation_trigger has no ==), so look inside those.
template<typename Type>
consteval auto is_deeply_equality_comparable() -> bool;

template<typename Type>
inline constexpr auto is_optional_v = false;

template<typename Type>
inline constexpr auto is_optional_v<std::optional<Type>> = true;

template<typename Type>
inline constexpr auto is_pair_v = false;

template<typename First, typename Second>
inline constexpr auto is_pair_v<std::pair<First, Second>> = true;

template<typename... Types>
consteval auto all_deeply_equality_comparable(std::variant<Types...>*) -> bool {
  return (is_deeply_equality_comparable<Types>() && ...);
}

template<typename Type>
consteval auto is_deeply_equality_comparable() -> bool {
  if constexpr (!std::equality_comparable<Type>) {
    return false;
  } else if constexpr (is_pair_v<Type>) {
    return is_deeply_equality_comparable<typename Type::first_type>() && is_deeply_equality_comparable<typename Type::second_type>();
  } else if constexpr (is_optional_v<Type>) {
    return is_deeply_equality_comparable<typename Type::value_type>();
  } else if constexpr (is_variant_v<Type>) {
    return all_deeply_equality_comparable(static_cast<Type*>(nullptr));
  } else if constexpr (std::ranges::range<Type>) {
    // Nested rather than &&, since range_value_t can't be named for a non-range; std::filesystem::path is a range of paths and would recurse forever.
    if constexpr (std::same_as<std::ranges::range_value_t<Type>, Type>) {
      return true;
    } else {
      return is_deeply_equality_comparable<std::ranges::range_value_t<Type>>();
    }
  } else {
    return true;
  }
}

} // namespace detail

/**
 * @brief Copies into target only what changed between before and after, so one Inspector edit reaches every selected node without overwriting untouched values.
 *
 * Recurses through structs (via reflection), arrays, a variant's held alternative and vector components, so dragging Y moves every node's Y only.
 * Anything else is compared and copied whole; types that can't be compared are skipped, since copying them blindly would clobber the other nodes' values.
 */
template<typename Type>
auto apply_changed_fields(const Type& before, const Type& after, Type& target) -> void {
  if constexpr (detail::math_vector<Type>) {
    constexpr auto size = decltype(detail::math_vector_size(before))::value;

    for (auto index = std::size_t{0u}; index < size; ++index) {
      if (before[index] != after[index]) {
        target[index] = after[index];
      }
    }
  } else if constexpr (std::is_array_v<Type>) {
    for (auto index = std::size_t{0u}; index < std::extent_v<Type>; ++index) {
      apply_changed_fields(before[index], after[index], target[index]);
    }
  } else if constexpr (detail::is_variant_v<Type>) {
    // Switching alternatives replaces the whole value; otherwise recurse, unless target holds a different alternative.
    if (before.index() != after.index()) {
      target = after;
    } else if (target.index() == after.index()) {
      [&]<std::size_t... Index>(std::index_sequence<Index...>) {
        ((after.index() == Index ? apply_changed_fields(std::get<Index>(before), std::get<Index>(after), std::get<Index>(target)) : void()), ...);
      }(std::make_index_sequence<std::variant_size_v<Type>>{});
    }
  } else if constexpr (std::is_aggregate_v<Type> && std::is_class_v<Type>) {
    constexpr auto context = std::meta::access_context::unchecked();

    template for (constexpr auto base : std::define_static_array(std::meta::bases_of(^^Type, context))) {
      using base_type = [:std::meta::type_of(base):];

      apply_changed_fields(static_cast<const base_type&>(before), static_cast<const base_type&>(after), static_cast<base_type&>(target));
    }

    template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(^^Type, context))) {
      apply_changed_fields(before.[:member:], after.[:member:], target.[:member:]);
    }
  } else if constexpr (detail::is_deeply_equality_comparable<Type>()) {
    if (!(before == after)) {
      target = after;
    }
  }
}

} // namespace editor

#endif // EDITOR_COMMANDS_FIELD_DIFF_HPP_
