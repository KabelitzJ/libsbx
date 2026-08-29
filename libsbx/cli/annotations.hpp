// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_CLI_ANNOTATIONS_HPP_
#define LIBSBX_CLI_ANNOTATIONS_HPP_

#include <meta>
#include <string_view>

#include <libsbx/reflection/annotations.hpp>

namespace sbx::cli {

struct required_t { };

inline constexpr auto required = required_t{};

struct help_t : reflection::string_annotation<help_t> {
  using string_annotation<help_t>::string_annotation;
}; // struct help_t

inline constexpr auto help = help_t{};

struct short_name_t {

  char value;

  consteval short_name_t()
  : value{'\0'} { }

  consteval short_name_t(char value)
  : value{value} { }

  consteval auto operator()(char value) const -> short_name_t {
    return short_name_t{value};
  }

}; // struct short_name_t

inline constexpr auto short_name = short_name_t{};

} // namespace sbx::cli

#endif // LIBSBX_CLI_ANNOTATIONS_HPP_
