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

template<typename Type>
struct string_annotation {

  const char* data;
  std::size_t size;

  consteval string_annotation()
  : data{nullptr},
    size{0} {}

  consteval string_annotation(std::string_view value)
  : data{std::define_static_string(value)},
    size{value.size()} { }

  consteval auto operator()(std::string_view value) const -> Type {
    return Type{value};
  }

  constexpr auto view() const noexcept -> std::string_view {
    return std::string_view{data, size};
  }

}; // struct annotation

struct rename_t : string_annotation<rename_t> {
  using string_annotation<rename_t>::string_annotation;
}; // struct rename_t

inline constexpr auto rename = rename_t{};

struct format_t : string_annotation<format_t> {
  using string_annotation<format_t>::string_annotation;
}; // struct format_t

inline constexpr auto format = format_t{};

struct range_t {

  std::size_t min;
  std::size_t max;

  consteval range_t()
  : min{0},
    max{0} {}

  consteval range_t(std::size_t min, std::size_t max)
  : min{min},
    max{max} { }

  consteval auto operator()(std::size_t min, std::size_t max) const -> range_t {
    return range_t{min, max};
  }

}; // struct range_t

inline constexpr auto range = range_t{};

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
