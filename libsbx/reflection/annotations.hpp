// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_ANNOTATIONS_HPP_
#define LIBSBX_REFLECTION_ANNOTATIONS_HPP_

#include <meta>
#include <string_view>

namespace sbx::reflection {

struct named_t { };

inline constexpr auto named = named_t{};

struct skip_t { };

inline constexpr auto skip = skip_t{};

struct bit_field_t { };

inline constexpr auto bit_field = bit_field_t{};

struct expose_t { };

inline constexpr auto expose = expose_t{};

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

template<typename Type, typename Annotation>
consteval auto has_annotation() -> bool {
  return !std::meta::annotations_of_with_type(^^Type, ^^Annotation).empty();
}

template<typename Type, auto Annotation>
consteval auto has_annotation() -> bool {
  return has_annotation<Type, decltype(Annotation)>();
}

template<typename Type, auto... Annotation>
consteval auto has_any_annotations(std::meta::info reflected) -> bool {
  return (has_annotation<Annotation>(reflected) || ...);
}

template<typename Type, auto... Annotation>
consteval auto has_all_annotations(std::meta::info reflected) -> bool {
  return (has_annotation<Annotation>(reflected) && ...);
}

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_ANNOTATIONS_HPP_
