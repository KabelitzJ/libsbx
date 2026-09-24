// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_ENUM_HPP_
#define LIBSBX_REFLECTION_ENUM_HPP_

#include <array>
#include <cstddef>
#include <functional>
#include <meta>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fmt/format.h>

#include <libsbx/reflection/annotations.hpp>

namespace sbx::reflection {

/** @brief An enum tagged `[[=reflection::named]]`, i.e. reflectable by name via to_string/from_string. */
template<typename Enum>
concept named_enum = std::meta::is_enum_type(^^Enum) && has_annotation<Enum, named>();

/** @return Enum's enumerator count. */
template<typename Enum>
requires (std::is_enum_v<Enum>)
consteval auto enum_count() -> std::size_t {
  return std::meta::enumerators_of(^^Enum).size();
}

/** @return Every value of Enum, in declaration order. */
template<typename Enum>
requires (std::is_enum_v<Enum>)
consteval auto enum_values() -> std::array<Enum, enum_count<Enum>()> {
  auto result = std::array<Enum, enum_count<Enum>()>{};
  auto index = std::size_t{0};

  template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Enum))) {
    result[index++] = [:e:];
  }

  return result;
}

/**
 * @brief Invokes callable with the name and value of every enumerator of Enum, in declaration order.
 *
 * @tparam Enum A named_enum.
 * @tparam Callable A callable invocable with (std::string_view, Enum).
 *
 * @param callable Invoked once per enumerator.
 */
template<named_enum Enum, typename Callable>
requires (std::is_invocable_v<Callable, std::string_view, Enum>)
constexpr auto for_each(Callable&& callable) -> void {
  template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Enum))) {
    std::invoke(callable, std::meta::identifier_of(e), [:e:]);
  }
}

/** @return value's enumerator name, or "<unknown>" if value doesn't match any enumerator. */
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

/** @return The enumerator named name, or std::nullopt if no enumerator has that name. */
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

/** @return The enumerator named name, or default_value if no enumerator has that name. */
template<named_enum Enum>
constexpr auto from_string_or(std::string_view name, const Enum default_value) -> Enum {
  auto result = from_string<Enum>(name);

  return result ? *result : default_value;
}

/** @return value reinterpreted as Enum. No validation that value matches a declared enumerator. */
template<typename Enum>
requires (std::is_enum_v<Enum>)
constexpr auto from_underlying(const std::underlying_type_t<Enum> value) -> Enum {
  return static_cast<Enum>(value);
}

/** @brief Whether Enum is tagged `[[=reflection::bit_field]]`, enabling the bitwise operators below for it. */
template<typename Enum>
requires (std::is_enum_v<Enum>)
inline constexpr auto is_bit_field_v = has_annotation<Enum, bit_field>();

} // namespace sbx::reflection

template<typename Type>
requires (sbx::reflection::is_bit_field_v<Type>)
constexpr auto operator|(Type lhs, Type rhs) -> Type {
  return sbx::reflection::from_underlying<Type>(std::to_underlying(lhs) | std::to_underlying(rhs));
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
  return sbx::reflection::from_underlying<Type>(std::to_underlying(lhs) & std::to_underlying(rhs));
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
  return sbx::reflection::from_underlying<Type>(std::to_underlying(lhs) ^ std::to_underlying(rhs));
}

template<typename Type>
requires (sbx::reflection::is_bit_field_v<Type>)
constexpr auto operator~(Type lhs) -> Type {
  return sbx::reflection::from_underlying<Type>(~std::to_underlying(lhs));
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
