// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_ANNOTATIONS_HPP_
#define LIBSBX_REFLECTION_ANNOTATIONS_HPP_

#include <meta>
#include <string_view>

#include <libsbx/utility/string_literal.hpp>

namespace sbx::reflection {

namespace detail {

struct named { };

struct skip { };

struct bit_field { };

struct expose { };

struct rename {

  const char* data;
  std::size_t size;

  consteval rename(std::string_view value)
  : data{std::define_static_string(value)},
    size{value.size()} { }

  constexpr auto view() const noexcept -> std::string_view {
    return std::string_view{data, size};
  }

}; // struct rename

struct format {

  const char* data;
  std::size_t size;

  consteval format(std::string_view value)
  : data{std::define_static_string(value)},
    size{value.size()} { }

  constexpr auto view() const noexcept -> std::string_view {
    return std::string_view{data, size};
  }

}; // struct rename

struct range {
  std::size_t min;
  std::size_t max;
}; // struct range

} // namespace detail

inline constexpr auto named = detail::named{};

inline constexpr auto skip = detail::skip{};

inline constexpr auto bit_field = detail::bit_field{};

template<utility::string_literal Name>
inline constexpr auto rename = detail::rename{Name};

template<utility::string_literal Format>
inline constexpr auto format = detail::format{Format};

template<std::size_t Min, std::size_t Max>
inline constexpr auto range = detail::range{Min, Max};

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_ANNOTATIONS_HPP_
