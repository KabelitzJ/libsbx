// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_ENUM_HPP_
#define LIBSBX_REFLECTION_ENUM_HPP_

#include <meta>
#include <optional>
#include <span>
#include <string_view>
#include <functional>

#include <libsbx/reflection/annotations.hpp>

namespace sbx::reflection {

template<typename Enum>
requires (std::is_enum_v<Enum>)
consteval auto enum_count() -> std::size_t {
  return std::meta::enumerators_of(^^Enum).size();
}

template<typename Enum>
concept reflected_enum = std::meta::is_enum_type(^^Enum) && !std::meta::annotations_of_with_type(^^Enum, std::meta::remove_cv(^^decltype(reflected))).empty();

template<reflected_enum Enum, typename Callable>
constexpr auto for_each(Callable&& callable) -> void {
  template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Enum))) {
    std::invoke(callable, std::meta::identifier_of(e), [:e:]);
  }
}

template<reflected_enum Enum>
constexpr auto to_string(Enum value) -> std::string_view {
  auto result = std::string_view{"<unknown>"};

  for_each<Enum>([&](auto name, auto entry) {
    if (entry == value) {
      result = name;
    }
  });

  return result;
}

template<reflected_enum Enum>
constexpr auto from_string(std::string_view name) -> std::optional<Enum> {
  auto result = std::optional<Enum>{};

  for_each<Enum>([&](auto entry_name, auto entry) {
    if (entry_name == name) {
      result = entry;
    }
  });

  return result;
}

template<reflected_enum Enum>
constexpr auto from_string_or(std::string_view name, const Enum default_value) -> Enum {
  auto result = from_string<Enum>(name);

  return result ? *result : default_value;
}

template<typename Enum>
requires (std::is_enum_v<Enum>)
constexpr auto to_underlying(const Enum value) -> std::underlying_type_t<Enum> {
  return static_cast<std::underlying_type_t<Enum>>(value);
}

template<typename Enum>
requires (std::is_enum_v<Enum>)
constexpr auto from_underlying(const std::underlying_type_t<Enum> value) -> Enum {
  return static_cast<Enum>(value);
}

template<typename Enum>
requires (std::is_enum_v<Enum>)
inline constexpr auto is_bit_field_v = !std::meta::annotations_of_with_type(^^Enum, std::meta::remove_cv(^^decltype(bit_field))).empty();

} // namespace sbx::reflection

template<typename Type>
requires (sbx::reflection::is_bit_field_v<Type>)
constexpr auto operator|(Type lhs, Type rhs) -> Type {
  return sbx::reflection::from_underlying<Type>(sbx::reflection::to_underlying(lhs) | sbx::reflection::to_underlying(rhs));
}

template<typename Type>
requires (sbx::reflection::is_bit_field_v<Type>)
constexpr auto operator|=(Type& lhs, Type rhs) -> Type& {
  lhs = lhs | rhs;

  return lhs;
}

template<typename Type>
requires (sbx::reflection::is_bit_field_v<Type>)
constexpr auto operator&(Type lhs, Type rhs) -> Type {
  return sbx::reflection::from_underlying<Type>(sbx::reflection::to_underlying(lhs) & sbx::reflection::to_underlying(rhs));
}

template<typename Type>
requires (sbx::reflection::is_bit_field_v<Type>)
constexpr auto operator&=(Type& lhs, Type rhs) -> Type& {
  lhs = lhs & rhs;

  return lhs;
}

template<typename Type>
requires (sbx::reflection::is_bit_field_v<Type>)
constexpr auto operator^(Type lhs, Type rhs) -> Type {
  return sbx::reflection::from_underlying<Type>(sbx::reflection::to_underlying(lhs) ^ sbx::reflection::to_underlying(rhs));
}

template<typename Type>
requires (sbx::reflection::is_bit_field_v<Type>)
constexpr auto operator~(Type lhs) -> Type {
  return sbx::reflection::from_underlying<Type>(~sbx::reflection::to_underlying(lhs));
}

#endif // LIBSBX_REFLECTION_ENUM_HPP_