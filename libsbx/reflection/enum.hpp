// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_ENUM_HPP_
#define LIBSBX_REFLECTION_ENUM_HPP_

#include <meta>
#include <optional>
#include <span>
#include <string_view>
#include <functional>
#include <type_traits>

#include <fmt/format.h>

#include <libsbx/reflection/annotations.hpp>

namespace sbx::reflection {

template<typename Enum>
concept named_enum = std::meta::is_enum_type(^^Enum) && has_annotation<Enum, named>();

template<typename Enum>
requires (std::is_enum_v<Enum>)
consteval auto enum_count() -> std::size_t {
  return std::meta::enumerators_of(^^Enum).size();
}

template<named_enum Enum, typename Callable>
requires (std::is_invocable_v<Callable, std::string_view, Enum>)
constexpr auto for_each(Callable&& callable) -> void {
  template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Enum))) {
    std::invoke(callable, std::meta::identifier_of(e), [:e:]);
  }
}

template<named_enum Enum>
constexpr auto to_string(const Enum value) -> std::string_view {
  auto result = std::string_view{"<unknown>"};

  for_each<Enum>([&](auto name, auto entry) {
    if (entry == value) {
      result = name;
    }
  });

  return result;
}

template<named_enum Enum>
constexpr auto from_string(std::string_view name) -> std::optional<Enum> {
  auto result = std::optional<Enum>{};

  for_each<Enum>([&](auto entry_name, auto entry) {
    if (entry_name == name) {
      result = entry;
    }
  });

  return result;
}

template<named_enum Enum>
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

template<sbx::reflection::named_enum Enum>
struct fmt::formatter<Enum> : public fmt::formatter<std::string_view> {

  using base_type = fmt::formatter<std::string_view>;

  template<typename FormatContext>
  auto format(const Enum& value, FormatContext& context) const -> decltype(auto) {
    return base_type::format(sbx::reflection::to_string(value), context);
  }

}; // struct fmt::formatter

template<typename Type>
requires (std::is_enum_v<Type> && !sbx::reflection::named_enum<Type>)
struct fmt::formatter<Type> : public fmt::formatter<std::underlying_type_t<Type>> {

  using base_type = fmt::formatter<std::underlying_type_t<Type>>;

  template<typename FormatContext>
  auto format(const Type& value, FormatContext& context) const -> decltype(auto) {
    return base_type::format(static_cast<std::underlying_type_t<Type>>(value), context);
  }

}; // struct fmt::formatter

#endif // LIBSBX_REFLECTION_ENUM_HPP_
