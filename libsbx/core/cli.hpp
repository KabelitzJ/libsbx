// SPDX-License-Identifier: MIT
#ifndef LIBSBX_CORE_CLI_HPP_
#define LIBSBX_CORE_CLI_HPP_

#include <string>
#include <string_view>
#include <span>
#include <vector>
#include <string_view>
#include <concepts>
#include <cinttypes>
#include <optional>
#include <charconv>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/string_literal.hpp>

namespace sbx::core {

template<utility::string_literal ShortName, utility::string_literal LongName, utility::string_literal Help = "">
struct flag {
 
  bool value = false;
 
  static constexpr auto short_name = std::string_view{ShortName};
  static constexpr auto long_name = std::string_view{LongName};
  static constexpr auto help = std::string_view{Help};

  static constexpr auto is_flag = true;
  static constexpr auto is_option = false;
  static constexpr auto is_positional = false;
  static constexpr auto is_required = false;
 
  operator bool() const { return value; }
 
}; // struct flag

template<typename Type, utility::string_literal ShortName, utility::string_literal LongName, utility::string_literal Help = "", bool IsRequired = false>
struct option {
 
  std::optional<Type> value;
 
  static constexpr auto short_name = std::string_view{ShortName};
  static constexpr auto long_name = std::string_view{LongName};
  static constexpr auto help = std::string_view{Help};

  static constexpr auto is_flag = false;
  static constexpr auto is_option = true;
  static constexpr auto is_positional = false;
  static constexpr auto is_required = IsRequired;
 
  using value_type = Type;
 
  auto get() const -> const Type& { return *value; }
  auto get_or(const Type& fallback) const -> Type { return value.value_or(fallback); }
  explicit operator bool() const { return value.has_value(); }
 
}; // struct option

template<typename Type, utility::string_literal Name, utility::string_literal Help = "", bool IsRequired = true>
struct positional {
 
  std::optional<Type> value;
 
  static constexpr auto short_name = std::string_view{};
  static constexpr auto long_name = std::string_view{Name};
  static constexpr auto help = std::string_view{Help};

  static constexpr auto is_flag = false;
  static constexpr auto is_option = false;
  static constexpr auto is_positional = true;
  static constexpr auto is_required = IsRequired;
 
  using value_type = Type;
 
  auto get() const -> const Type& { return *value; }
  explicit operator bool() const { return value.has_value(); }
 
}; // positional

template<typename... Types>
using argument_list = std::tuple<Types...>;

namespace detail {

inline static constexpr auto invalid_argument_index = ~size_t{0};
 
template<utility::string_literal Name, typename Tuple, size_t... Indices>
constexpr auto index_of(std::index_sequence<Indices...>) -> size_t {
  constexpr auto matches = std::array{(std::tuple_element_t<Indices, Tuple>::long_name == std::string_view{Name})...};

  for (auto i = size_t{0}; i < sizeof...(Indices); ++i) {
    if (matches[i]) { 
      return i; 
    }
  }

  return invalid_argument_index;
}

template<typename Type>
auto parse_value(std::string_view view) -> Type {
  if constexpr (std::is_same_v<Type, std::string>) {
    return std::string{view};
  } else if constexpr (std::is_integral_v<Type>) {
    auto result = Type{};

    auto [ptr, ec] = std::from_chars(view.data(), view.data() + view.size(), result);

    if (ec != std::errc{}) {
      throw std::invalid_argument{std::format("cannot parse '{}' as integer", view)};
    }

    return result;
  } else if constexpr (std::is_floating_point_v<Type>) {
    auto result = Type{};

    auto [ptr, ec] = std::from_chars(view.data(), view.data() + view.size(), result);

    if (ec != std::errc{}) {
      throw std::invalid_argument{std::format("cannot parse '{}' as float", view)};
    }

    return result;
  } else {
    static_assert(sizeof(Type) == 0, "unsupported value type");
  }
}
 
// Iterate tuple elements at compile time via index sequence
template<typename Tuple, typename Function, size_t... Indices>
auto tuple_for_each(Tuple& tuple, Function&& function, std::index_sequence<Indices...>) -> void {
  (function(std::get<Indices>(tuple), std::integral_constant<size_t, Indices>{}), ...);
}
 
template<typename Tuple, typename Function>
auto tuple_for_each(Tuple& tuple, Function&& function) -> void {
  tuple_for_each(tuple, std::forward<Function>(function), std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}
 
} // namespace detail

template<utility::string_literal Name, typename... Types>
auto get(std::tuple<Types...>& tuple) -> auto& {
  constexpr auto index = detail::index_of<Name, std::tuple<Types...>>(std::index_sequence_for<Types...>{});

  static_assert(index != detail::invalid_argument_index, "no argument with that name");

  return std::get<index>(tuple);
}
 
template<utility::string_literal Name, typename... Types>
auto get(const std::tuple<Types...>& tuple) -> const auto& {
  constexpr auto index = detail::index_of<Name, std::tuple<Types...>>(std::index_sequence_for<Types...>{});

  static_assert(index != detail::invalid_argument_index, "no argument with that name");

  return std::get<index>(tuple);
}

struct parse_error : std::runtime_error {
  using std::runtime_error::runtime_error;
}; // struct parse_error
 
template<typename ArgumentList>
struct parser {
 
public:
 
  explicit parser(std::string_view program_name)
  : _program_name{program_name} { }
 
  auto parse(int argc, char** argv) -> ArgumentList {
    auto tokens = std::vector<std::string_view>{};
    tokens.reserve(static_cast<std::size_t>(argc - 1));
 
    for (auto i = 1; i < argc; ++i) {
      tokens.emplace_back(argv[i]);
    }
 
    return _parse(std::span<std::string_view>{tokens});
  }
 
  auto parse(std::span<std::string_view> tokens) -> ArgumentList {
    return _parse(tokens);
  }
 
  auto help_string() const -> std::string {
    return _build_help();
  }
 
  auto print_help() const -> void {
    utility::logger<"core">::info("{}", _build_help());
  }
 
private:
 
  auto _parse(std::span<std::string_view> tokens) -> ArgumentList {
    auto result  = ArgumentList{};
    auto pos_idx = size_t{0};
 
    for (auto i = size_t{0}; i < tokens.size(); ++i) {
      auto tok = tokens[i];
 
      if (tok == "--help" || tok == "-h") {
        print_help();
        std::exit(0);
      }
 
      if (tok.starts_with("--") || (tok.starts_with("-") && tok.size() == 2)) {
        auto tok_name = tok.starts_with("--") ? tok.substr(2) : tok.substr(1);
        auto eq_pos   = tok_name.find('=');
        auto key      = tok_name.substr(0, eq_pos);
        auto has_inline_val = eq_pos != std::string_view::npos;
        auto matched  = false;
 
        detail::tuple_for_each(result, [&](auto& field, auto) {
          using FieldType = std::remove_reference_t<decltype(field)>;
 
          auto name_match =
            (!FieldType::short_name.empty() && key == FieldType::short_name) ||
            (!FieldType::long_name.empty()  && key == FieldType::long_name);
 
          if (!name_match) { return; }
 
          matched = true;
 
          if constexpr (FieldType::is_flag) {
            field.value = true;
          } else if constexpr (FieldType::is_option) {
            auto sv = std::string_view{};
            if (has_inline_val) {
              sv = tok_name.substr(eq_pos + 1);
            } else {
              if (i + 1 >= tokens.size()) {
                throw parse_error{std::format("option '--{}' requires a value", key)};
              }
              sv = tokens[++i];
            }
            field.value = detail::parse_value<typename FieldType::value_type>(sv);
          }
        });
 
        if (!matched) {
          throw parse_error{std::format("unknown option '{}'", tok)};
        }
 
      } else {
        auto handled = false;
        auto cur_pos = size_t{0};
 
        detail::tuple_for_each(result, [&](auto& field, auto) {
          using FieldType = std::remove_reference_t<decltype(field)>;
 
          if constexpr (!FieldType::is_positional) { return; }
 
          if (cur_pos == pos_idx && !handled) {
            field.value = detail::parse_value<typename FieldType::value_type>(tok);
            handled = true;
          }
 
          ++cur_pos;
        });
 
        if (!handled) {
          throw parse_error{std::format("unexpected positional argument '{}'", tok)};
        }
 
        ++pos_idx;
      }
    }
 
    _check_required(result);
 
    return result;
  }
 
  auto _check_required(const ArgumentList& result) const -> void {
    detail::tuple_for_each(const_cast<ArgumentList&>(result), [](const auto& field, auto) {
      using FieldType = std::remove_cvref_t<decltype(field)>;
 
      if constexpr (FieldType::is_required) {
        if (!field.value.has_value()) {
          throw parse_error{std::format("required argument '{}' not provided", FieldType::long_name)};
        }
      }
    });
  }
 
  auto _build_help() const -> std::string {
    auto out = fmt::format("Usage: {}", _program_name);
    auto pos_synopsis = std::string{};
    auto opts_help = std::string{};
    auto pos_help = std::string{};
 
    auto dummy = ArgumentList{};
 
    detail::tuple_for_each(dummy, [&](const auto& field, auto) {
      using FieldType = std::remove_cvref_t<decltype(field)>;
 
      if constexpr (FieldType::is_positional) {
        pos_synopsis += FieldType::required ? std::format(" <{}>", FieldType::long_name) : std::format(" [{}]", FieldType::long_name);
        pos_help += std::format("  {:20}  {}\n", FieldType::long_name, FieldType::help);
      } else if constexpr (FieldType::is_flag) {
        opts_help += std::format("  -{}, --{:16}  {}\n", FieldType::short_name.empty() ? " " : FieldType::short_name, FieldType::long_name, FieldType::help);
      } else if constexpr (FieldType::is_option) {
        auto flag_str = std::format("--{} <value>", FieldType::long_name);
        opts_help += std::format("  {:22}  {}{}\n", flag_str, FieldType::help, FieldType::required ? " [required]" : "");
      }
    });
 
    out += pos_synopsis;
    out += "\n\nOptions:\n";
    out += "  -h, --help              Show this help message\n";
    out += opts_help;
 
    if (!pos_help.empty()) {
      out += "\nPositional arguments:\n";
      out += pos_help;
    }
 
    return out;
  }

  std::string _program_name;
 
}; // class parser

template<typename ArgumentList>
auto parse(int argc, char** argv, std::string_view program_name = "program") -> ArgumentList {
  return parser<ArgumentList>{program_name}.parse(argc, argv);
}

} // namespace sbx::core

#endif // LIBSBX_CORE_CLI_HPP_
