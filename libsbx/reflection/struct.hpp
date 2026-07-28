// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_STRUCT_HPP_
#define LIBSBX_REFLECTION_STRUCT_HPP_

#include <meta>
#include <string_view>

#include <libsbx/reflection/annotations.hpp>

namespace sbx::reflection {

namespace detail {

consteval auto display_name_of(std::meta::info member) -> std::string_view {
  auto renames = std::meta::annotations_of_with_type(member, ^^detail::rename);

  return (!renames.empty()) ? std::meta::extract<detail::rename>(renames.front()).view() : std::meta::identifier_of(member);
}

consteval auto format_specifier_of(std::meta::info member) -> std::optional<std::string_view> {
  auto formats = std::meta::annotations_of_with_type(member, ^^detail::format);

  return (!formats.empty()) ? std::optional{std::meta::extract<detail::format>(formats.front()).view()} : std::nullopt;
}

} // namespace detail

template<typename Type>
concept named_struct = std::meta::is_class_type(^^Type) && !std::meta::annotations_of_with_type(^^Type, std::meta::remove_cv(^^detail::named)).empty();

template<named_struct Type>
auto to_string(const Type& value) -> std::string {
  constexpr auto type_info = ^^Type;

  auto result = std::string{};
  result.reserve(32);

  result += detail::display_name_of(type_info);

  result += "{ ";

  auto is_first = true;

  template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(type_info, std::meta::access_context::unchecked()))) {
    if constexpr (std::meta::annotations_of_with_type(member, ^^detail::skip).empty()) {
      if (!is_first) {
        result += ", ";
      }

      is_first = false;

      result += ".";
      result += detail::display_name_of(member);
      result += ": ";

      using member_type = [:std::meta::type_of(member):];

      const auto& field = value.[:member:];

      if constexpr (named_struct<member_type> || named_enum<member_type>) {
        result += to_string(field);
      } else {
        constexpr auto specifier = detail::format_specifier_of(member);

        if constexpr (specifier) {
          result += fmt::vformat(fmt::format("{{:{}}}", *specifier), fmt::make_format_args(field));
        } else {
          result += fmt::format("{}", field);
        }
      }
    }
  }

  result += " }";

  return result;
}

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_STRUCT_HPP_
