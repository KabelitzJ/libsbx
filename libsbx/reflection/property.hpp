// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_PROPERTY_HPP_
#define LIBSBX_REFLECTION_PROPERTY_HPP_

#include <span>
#include <vector>
#include <string_view>
#include <type_traits>
#include <utility>
#include <meta>

#include <libsbx/reflection/annotations.hpp>

namespace sbx::reflection {

template<typename Type>
concept reflectable =
    std::meta::is_class_type(^^Type) &&
    (!std::meta::annotations_of_with_type(^^Type, ^^reflected).empty() ||
     (std::meta::has_template_arguments(^^Type) &&
      !std::meta::annotations_of_with_type(
          std::meta::template_of(^^Type), ^^reflected).empty()));

template<std::meta::info Member>
struct field_property {

  static consteval auto name() -> std::string_view {
    return std::meta::identifier_of(Member);
  }

  static consteval auto reflection() -> std::meta::info {
    return Member;
  }

  static consteval auto is_writable() -> bool {
    return true;
  }

  static constexpr auto get(auto&& object) -> decltype(auto) {
    return (std::forward<decltype(object)>(object).[:Member:]);
  }

  static constexpr auto set(auto&& object, auto&& value) -> void {
    object.[:Member:] = std::forward<decltype(value)>(value);
  }

  template<typename Annotation>
  static consteval auto has() -> bool {
    return !std::meta::annotations_of_with_type(Member, ^^Annotation).empty();
  }

}; // struct field_property

template<std::meta::info Getter, std::meta::info Setter>
struct accessor_property {

  static consteval auto name() -> std::string_view {
    return std::meta::identifier_of(Getter);
  }

  static consteval auto reflection() -> std::meta::info {
    return Getter;
  }

  static consteval auto is_writable() -> bool {
    return Setter != std::meta::info{};
  }

  static constexpr auto get(auto&& object) -> decltype(auto) {
    return object.[:Getter:]();
  }

  static constexpr auto set(auto&& object, auto&& value) -> void {
    if constexpr (Setter != std::meta::info{}) {
      object.[:Setter:]() = std::forward<decltype(value)>(value);
    }
  }

  template<typename Annotation>
  static consteval auto has() -> bool {
    return !std::meta::annotations_of_with_type(Getter, ^^Annotation).empty();
  }

}; // struct accessor_property

namespace detail {

template<typename Type>
consteval auto data_members() -> std::span<const std::meta::info> {
  return std::define_static_array(std::meta::nonstatic_data_members_of(^^Type, std::meta::access_context::unchecked()));
}

template<typename Type>
consteval auto exposed_getters() -> std::span<const std::meta::info> {
  auto out = std::vector<std::meta::info>{};

  for (auto member : std::meta::members_of(^^Type, std::meta::access_context::unchecked())) {
    if (std::meta::is_function(member) && std::meta::is_const(member) && !std::meta::annotations_of_with_type(member, ^^expose).empty()) {
      out.push_back(member);
    }
  }

  return std::define_static_array(out);
}

template<typename Type>
consteval auto mutable_overload(std::string_view name) -> std::meta::info {
  for (auto member : std::meta::members_of(^^Type, std::meta::access_context::unchecked())) {
    if (std::meta::is_function(member) && !std::meta::is_const(member) && std::meta::identifier_of(member) == name && std::meta::parameters_of(member).empty()) {
      return member;
    }
  }

  return std::meta::info{};
}

} // namespace detail

template<typename Type, typename Callable>
constexpr auto for_each_property(Type&& object, Callable&& callable) -> void {
  using clean = std::remove_cvref_t<Type>;

  template for (constexpr auto member : detail::data_members<clean>()) {
    callable(field_property<member>{}, object);
  }

  template for (constexpr auto getter : detail::exposed_getters<clean>()) {
    constexpr auto setter = detail::mutable_overload<clean>(std::meta::identifier_of(getter));

    callable(accessor_property<getter, setter>{}, object);
  }
}

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_PROPERTY_HPP_
