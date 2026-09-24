// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_REFLECTION_ANNOTATIONS_HPP_
#define LIBSBX_REFLECTION_ANNOTATIONS_HPP_

#include <cstddef>
#include <meta>
#include <string_view>

namespace sbx::reflection {

/** @brief Tags a type/enum as reflectable by name, e.g. `enum class [[=reflection::named]] format { ... };`. See named_enum, named_struct. */
struct named_t { };

inline constexpr auto named = named_t{};

/** @brief Marks a struct member to be skipped by reflection::to_string. */
struct skip_t { };

inline constexpr auto skip = skip_t{};

/** @brief Tags an enum as a bitmask, enabling operator|, operator&, operator^, operator~, operator|= and operator&= for it. See is_bit_field_v. */
struct bit_field_t { };

inline constexpr auto bit_field = bit_field_t{};

/** @brief Marks a struct member as exposed (e.g. to the editor inspector or scripting). */
struct expose_t { };

inline constexpr auto expose = expose_t{};

/**
 * @brief Base for annotation types that carry a single string, e.g. rename_t, format_t.
 *
 * @tparam Type The derived annotation type, returned by operator().
 *
 * @note data/size are public, not an implementation detail — an annotation attached via `[[=some_annotation(...)]]` must be a structural type (all public members) to be usable where C++26 annotations require one.
 */
template<typename Type>
struct string_annotation {

  const char* data;
  std::size_t size;

  consteval string_annotation()
  : data{nullptr},
    size{0} {}

  /**
   * @brief Constructs from a string, copying it into static storage so it outlives the
   * consteval context that builds it.
   *
   * @param value The string to store.
   */
  consteval string_annotation(std::string_view value)
  : data{std::define_static_string(value)},
    size{value.size()} { }

  /**
   * @brief Builds a Type annotation value from a string, for use as `[[=some_annotation("...")]]`.
   *
   * @param value The string to store.
   *
   * @return The constructed annotation value.
   */
  consteval auto operator()(std::string_view value) const -> Type {
    return Type{value};
  }

  /** @return The stored string. */
  constexpr auto view() const noexcept -> std::string_view {
    return std::string_view{data, size};
  }

}; // struct string_annotation

/** @brief Renames a struct member for reflection::to_string, e.g. `[[=reflection::rename("id")]]`. */
struct rename_t : string_annotation<rename_t> {
  using string_annotation<rename_t>::string_annotation;
}; // struct rename_t

inline constexpr auto rename = rename_t{};

/** @brief Overrides a struct member's fmt format spec in reflection::to_string, e.g. `[[=reflection::format(".2f")]]`. */
struct format_t : string_annotation<format_t> {
  using string_annotation<format_t>::string_annotation;
}; // struct format_t

inline constexpr auto format = format_t{};

/** @brief A numeric range annotation, e.g. for an editor inspector slider's min/max. */
struct range_t {

  std::size_t min;
  std::size_t max;

  consteval range_t()
  : min{0},
    max{0} {}

  /**
   * @brief Constructs from explicit bounds.
   *
   * @param min The lower bound.
   * @param max The upper bound.
   */
  consteval range_t(std::size_t min, std::size_t max)
  : min{min},
    max{max} { }

  /**
   * @brief Builds a range_t annotation value, for use as `[[=reflection::range(0, 100)]]`.
   *
   * @param min The lower bound.
   * @param max The upper bound.
   *
   * @return The constructed annotation value.
   */
  consteval auto operator()(std::size_t min, std::size_t max) const -> range_t {
    return range_t{min, max};
  }

}; // struct range_t

inline constexpr auto range = range_t{};

/**
 * @brief Whether Type has an annotation of type Annotation attached (e.g. `[[=reflection::named]]`).
 *
 * @tparam Type The reflected type to check.
 * @tparam Annotation The annotation type to look for.
 *
 * @return true if Type has at least one Annotation-typed annotation.
 */
template<typename Type, typename Annotation>
consteval auto has_annotation() -> bool {
  return !std::meta::annotations_of_with_type(^^Type, ^^Annotation).empty();
}

/**
 * @brief Whether Type has an annotation of Annotation's type attached, e.g.
 * `has_annotation<my_enum, named>()`.
 *
 * @tparam Type The reflected type to check.
 * @tparam Annotation An annotation value; only its type is used.
 *
 * @return true if Type has at least one annotation of decltype(Annotation).
 */
template<typename Type, auto Annotation>
consteval auto has_annotation() -> bool {
  return has_annotation<Type, decltype(Annotation)>();
}

/**
 * @brief Whether a reflected entity (e.g. a struct member from nonstatic_data_members_of) has
 * at least one of several annotation types attached.
 *
 * @tparam Annotation The annotation values to check for; only their types are used.
 *
 * @param reflected The entity to check.
 *
 * @return true if reflected has an annotation matching the type of any of Annotation.
 */
template<auto... Annotation>
consteval auto has_any_annotations(std::meta::info reflected) -> bool {
  return (!std::meta::annotations_of_with_type(reflected, ^^decltype(Annotation)).empty() || ...);
}

/**
 * @brief Whether a reflected entity (e.g. a struct member from nonstatic_data_members_of) has
 * every one of several annotation types attached.
 *
 * @tparam Annotation The annotation values to check for; only their types are used.
 *
 * @param reflected The entity to check.
 *
 * @return true if reflected has an annotation matching the type of every one of Annotation.
 */
template<auto... Annotation>
consteval auto has_all_annotations(std::meta::info reflected) -> bool {
  return (!std::meta::annotations_of_with_type(reflected, ^^decltype(Annotation)).empty() && ...);
}

} // namespace sbx::reflection

#endif // LIBSBX_REFLECTION_ANNOTATIONS_HPP_
