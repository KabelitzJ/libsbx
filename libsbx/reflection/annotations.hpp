// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_ANNOTATIONS_HPP_
#define LIBSBX_REFLECTION_ANNOTATIONS_HPP_

#include <meta>

namespace sbx::reflection {

struct reflected { };

struct skip { };

struct bit_field { };

struct expose { };

template<std::size_t Size>
struct rename {

  std::array<char, Size - 1> data;

  consteval rename(const char (&string)[Size]) {
    for (auto i = std::size_t{0}; i < Size - 1; ++i) {
      data[i] = string[i];
    }
  }

  constexpr auto view() const noexcept -> std::string_view {
    return {data.data(), data.size()};
  }

}; // struct rename

template<std::size_t Size>
struct format {

  std::array<char, Size - 1> data;

  consteval format(const char (&string)[Size]) {
    for (auto i = std::size_t{0}; i < Size - 1; ++i) {
      data[i] = string[i];
    }
  }

  constexpr auto view() const noexcept -> std::string_view {
    return {data.data(), data.size()};
  }

}; // struct format


} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_ANNOTATIONS_HPP_
