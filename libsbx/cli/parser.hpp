// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CLI_PARSER_HPP_
#define LIBSBX_CLI_PARSER_HPP_

#include <meta>
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <expected>

#include <fmt/format.h>

#include <libsbx/reflection/reflection.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/type_name.hpp>

#include <libsbx/cli/annotations.hpp>
#include <libsbx/cli/traits.hpp>

namespace sbx::cli {

template<typename Type>
concept args_struct = std::meta::is_class_type(^^Type) && reflection::has_annotation<Type, args>();

struct parse_error {

  std::vector<std::string> messages;

  auto ok() const noexcept -> bool {
    return messages.empty();
  }

}; // struct parse_error

namespace detail {

consteval auto flag_name_of(std::meta::info member) -> std::string_view {
  auto renames = std::meta::annotations_of_with_type(member, ^^reflection::rename_t);

  auto name = (!renames.empty())
    ? std::meta::extract<reflection::rename_t>(renames.front()).view()
    : std::meta::identifier_of(member);

  auto buffer = std::string{"--"};
  buffer += name;

  return std::define_static_string(buffer);
}

consteval auto short_flag_of(std::meta::info member) -> std::optional<std::string_view> {
  auto shorts = std::meta::annotations_of_with_type(member, ^^short_name_t);

  if (shorts.empty()) {
    return std::nullopt;
  }

  auto buffer = std::string{"-"};
  buffer += std::meta::extract<short_name_t>(shorts.front()).value;

  return std::string_view{std::define_static_string(buffer)};
}

consteval auto help_of(std::meta::info member) -> std::string_view {
  auto helps = std::meta::annotations_of_with_type(member, ^^help_t);

  return (!helps.empty()) ? std::meta::extract<help_t>(helps.front()).view() : std::string_view{};
}

consteval auto is_required(std::meta::info member) -> bool {
  return !std::meta::annotations_of_with_type(member, ^^required_t).empty();
}

template<typename Type>
auto from_chars_into(std::string_view text) -> std::optional<Type> {
  auto value = Type{};

  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);

  if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
    return std::nullopt;
  }

  return value;
}

template<typename Type>
auto assign(Type& field, std::string_view text, std::string_view flag, std::vector<std::string>& errors) -> void {
  if constexpr (std::same_as<Type, std::string>) {
    field = std::string{text};
  } else if constexpr (std::same_as<Type, std::string_view>) {
    field = text;
  } else if constexpr (std::same_as<Type, std::filesystem::path>) {
    field = std::filesystem::path{text};
  } else if constexpr (reflection::named_enum<Type>) {
    if (const auto value = reflection::from_string<Type>(text)) {
      field = *value;
    } else {
      errors.push_back(fmt::format("invalid value '{}' for '{}'", text, flag));
    }
  } else if constexpr (std::integral<Type> || std::floating_point<Type>) {
    if (const auto value = from_chars_into<Type>(text)) {
      field = *value;
    } else {
      errors.push_back(fmt::format("invalid value '{}' for '{}' (expected {})", text, flag, utility::type_name<Type>()));
    }
  } else if constexpr (is_optional_v<Type>) {
    auto inner = typename Type::value_type{};
    assign(inner, text, flag, errors);
    field = inner;
  } else if constexpr (is_vector_v<Type>) {
    auto inner = typename Type::value_type{};
    assign(inner, text, flag, errors);
    field.push_back(std::move(inner));
  } else {
    static_assert(sizeof(Type) == 0, "sbx::cli: unsupported field type for CLI parsing");
  }
}

} // namespace detail

template<args_struct Type>
auto parse(std::span<const std::string_view> args) -> std::expected<Type, parse_error> {
  constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^Type, std::meta::access_context::unchecked()));

  auto result = Type{};
  auto errors = std::vector<std::string>{};
  auto seen = std::array<bool, members.size()>{};

  // args[0] is the program's own invocation path (argv[0]), never a flag — skip it.
  const auto start = args.empty() ? std::size_t{0u} : std::size_t{1u};

  for (auto index = start; index < args.size();) {
    const auto arg = args[index];
    auto matched = false;

    template for (constexpr auto i : std::define_static_array(std::views::iota(std::size_t{0u}, members.size()))) {
      constexpr auto member = members[i];

      if constexpr (std::meta::annotations_of_with_type(member, ^^reflection::skip_t).empty()) {
        constexpr auto flag = detail::flag_name_of(member);
        constexpr auto short_flag = detail::short_flag_of(member);

        if (!matched && (arg == flag || (short_flag && arg == *short_flag))) {
          using member_type = [:std::meta::type_of(member):];

          if constexpr (std::same_as<member_type, bool>) {
            result.[:member:] = true;
            index += 1u;
          } else {
            if (index + 1u >= args.size()) {
              errors.push_back(fmt::format("missing value for '{}'", flag));
              index += 1u;
            } else {
              detail::assign(result.[:member:], args[index + 1u], flag, errors);
              index += 2u;
            }
          }

          seen[i] = true;
          matched = true;
        }
      }
    }

    if (!matched) {
      errors.push_back(fmt::format("unknown argument '{}'", arg));
      index += 1u;
    }
  }

  template for (constexpr auto i : std::define_static_array(std::views::iota(std::size_t{0u}, members.size()))) {
    constexpr auto member = members[i];

    if constexpr (detail::is_required(member)) {
      using member_type = [:std::meta::type_of(member):];

      static_assert(!is_optional_v<member_type>, "sbx::cli: a field cannot be both required and std::optional<T> — use plain T");

      if (!seen[i]) {
        errors.push_back(fmt::format("missing required argument '{}'", detail::flag_name_of(member)));
      }
    }
  }

  if (!errors.empty()) {
    return std::unexpected{parse_error{std::move(errors)}};
  }

  return result;
}

template<args_struct Type>
auto usage_string(std::string_view program_name) -> std::string {
  static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^Type, std::meta::access_context::unchecked()));

  auto result = fmt::format("usage: {} [options]\n\noptions:\n", program_name);

  template for (constexpr auto member : members) {
    if constexpr (std::meta::annotations_of_with_type(member, ^^reflection::skip_t).empty()) {
      constexpr auto flag = detail::flag_name_of(member);
      constexpr auto short_flag = detail::short_flag_of(member);
      constexpr auto text = detail::help_of(member);
      constexpr auto required = detail::is_required(member);

      result += fmt::format("  {:<20} {}{}{}\n", flag, short_flag ? fmt::format("({}) ", *short_flag) : std::string{}, text, required ? " (required)" : "");
    }
  }

  result += "  --help, -h           show this message\n";

  return result;
}

template<args_struct Type>
auto parse_or_exit(std::span<const std::string_view> args) -> Type {
  const auto program_name = args.empty() ? std::string{"<program>"} : std::filesystem::path{args.front()}.filename().string();

  if (std::ranges::any_of(args, [](const auto arg) { return arg == "--help" || arg == "-h"; })) {
    fmt::print("{}", usage_string<Type>(program_name));
    std::exit(0);
  }

  auto result = parse<Type>(args);

  if (!result) {
    for (const auto& message : result.error().messages) {
      utility::logger<"cli">::error("{}", message);
    }

    fmt::print("\n{}", usage_string<Type>(program_name));
    std::exit(1);
  }

  return *result;
}

} // namespace sbx::cli

#endif // LIBSBX_CLI_PARSER_HPP_
