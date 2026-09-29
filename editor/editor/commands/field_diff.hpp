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

// Declaration only -- deduces a basic_vector base's size in unevaluated contexts.
template<std::size_t Size, typename Type>
auto math_vector_size(const sbx::math::basic_vector<Size, Type>&) -> std::integral_constant<std::size_t, Size>;

template<typename Type>
concept math_vector = requires(const Type& value) { detail::math_vector_size(value); };

template<typename Type>
inline constexpr auto is_variant_v = false;

template<typename... Types>
inline constexpr auto is_variant_v<std::variant<Types...>> = true;

// std::equality_comparable alone isn't enough: libstdc++'s == for vector/pair/optional/variant is unconstrained on the element
// type, so the concept says yes and the comparison only fails once instantiated (animator's parameter list holds an
// animation_trigger, which has no ==). Look inside those wrappers before trusting ==.
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
    // Nested, not &&: range_value_t can't even be named for a non-range. std::filesystem::path is a range of paths, which
    // would recurse forever.
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
 * @brief Copies into target only what changed between before and after -- how one edit on the Inspector's primary node reaches
 * every other selected node without overwriting the values that edit didn't touch.
 *
 * Recurses through plain structs (members and bases, via reflection), arrays, the held alternative of a variant, and into math
 * vectors per component, so dragging just Y of a position moves every node's Y and leaves their X/Z alone. Anything else (asset
 * handles, containers, strings, quaternions) is compared whole with == and copied whole if it changed. A type that can't be compared is skipped: there's no
 * telling whether it changed, and copying it blindly would overwrite the other nodes' own values.
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
    // Switching alternatives (e.g. a collider's Box -> Sphere) replaces the whole value; otherwise recurse into the held
    // alternative -- unless target holds a different one, which this edit then doesn't apply to.
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
