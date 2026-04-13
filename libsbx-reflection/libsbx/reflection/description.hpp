// SPDX-License-Identifier: MIT
#ifndef LIBSBX_REFLECTION_DESCROPTION_HPP_
#define LIBSBX_REFLECTION_DESCROPTION_HPP_

#include <concepts>
#include <optional>
#include <string_view>
#include <tuple>

#include <libsbx/reflection/member.hpp>
#include <libsbx/reflection/enumerator.hpp>

namespace sbx::reflection {

template<typename Type>
struct description;

template<typename Type>
concept reflectable_struct = std::is_class_v<Type> && requires() {
  { description<Type>::name() } -> std::convertible_to<std::string_view>;
  { description<Type>::members() };
}; // concept reflectable_struct

template<typename Type>
concept reflectable_enum = std::is_enum_v<Type> && requires() {
  { description<Type>::name() } -> std::convertible_to<std::string_view>;
  { description<Type>::enumerators() };
}; // concept reflectable_struct

template<typename Type>
concept reflectable = reflectable_struct<Type> || reflectable_enum<Type>;

template<reflectable_struct Type, typename Callback>
auto for_each(Type& instance, Callback&& callback) -> void {
  constexpr auto members = description<Type>::members();

  std::apply([&](auto const&... field) {
    (callback(field.name, instance.*(field.pointer)), ...);
  }, members);
}

template<reflectable_struct Type, typename Callback>
auto for_each(Type const& instance, Callback&& callback) -> void {
  constexpr auto members = description<Type>::members();

  std::apply([&](auto const&... field) {
    (callback(field.name, instance.*(field.pointer)), ...);
  }, members);
}

template<reflectable_struct Type, typename Callback>
auto visit_member(Type& instance, std::string_view name, Callback&& callback) -> bool {
  auto found = false;

  for_each(instance, [&](auto member_name, auto& value) {
    if (!found && member_name == name) {
      callback(value);
      found = true;
    }
  });

  return found;
}

template<reflectable_enum Enum, typename Callback>
auto for_each(Callback&& callback) -> void {
  constexpr auto enumerators = description<Enum>::enumerators();

  std::apply([&](auto const&... entry) {
    (callback(entry.name, entry.value), ...);
  }, enumerators);
}

template<reflectable_enum Enum>
auto to_string(Enum value) -> std::string_view {
  auto result = std::string_view{};

  for_each<Enum>([&](auto name, auto entry) {
    if (entry == value) {
      result = name;
    }
  });

  return result;
}

template<reflectable_enum Enum>
auto from_string(std::string_view name) -> std::optional<Enum> {
  auto result = std::optional<Enum>{};

  for_each<Enum>([&](auto entry_name, auto entry) {
    if (entry_name == name) {
      result = entry;
    }
  });

  return result;
}

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_DESCROPTION_HPP_