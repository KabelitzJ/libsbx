// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UTILITY_ITERATOR_HPP_
#define LIBSBX_UTILITY_ITERATOR_HPP_

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <algorithm>

#include <range/v3/all.hpp>

namespace sbx::utility {

template<typename Category, typename Type, typename Distance = std::ptrdiff_t, typename Pointer = Type*, typename Reference = Type&>
struct iterator {
  using iterator_category = Category;
  using value_type = Type;
  using difference_type = Distance;
  using pointer = Pointer;
  using reference = Reference;
}; // struct iterator

template<typename Type>
concept iterable = requires(Type t) {
  { std::begin(t) } -> std::same_as<typename Type::iterator>;
  { std::end(t) } -> std::same_as<typename Type::iterator>;
} || requires(Type t) {
  { std::begin(t) } -> std::same_as<typename Type::const_iterator>;
  { std::end(t) } -> std::same_as<typename Type::const_iterator>;
} || std::is_array_v<Type>;

template<template<typename> typename To, ranges::input_range Range, std::invocable<const ranges::range_value_t<Range>&> Fn>
requires (ranges::output_range<To<std::invoke_result_t<Fn, const ranges::range_value_t<Range>&>>, std::invoke_result_t<Fn, const ranges::range_value_t<Range>&>>)
auto map_to(Range&& range, Fn&& fn) -> To<std::invoke_result_t<Fn, const ranges::range_value_t<Range>&>> {
  return std::forward<Range>(range) | ranges::views::transform(std::forward<Fn>(fn)) | ranges::to<To>();
}

template<typename Range>
concept move_appendable = std::movable<typename Range::value_type> && requires(Range& destination, Range& source, std::size_t n) {
  source.size();
  source.clear();
  destination.size();
  destination.reserve(n);
  destination.insert(destination.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
}; // concept move_appendable

/**
 * @brief Appends elements from a source vector to a destination vector by moving them.
 *
 * @tparam Type A movable type stored in the vector.
 *
 * @param destination The vector to which elements will be appended.
 * @param source The vector from which elements will be moved. It will be cleared after the operation.
 *
 * @note The source vector will be left empty after this call.
 */
template<move_appendable Range>
auto append(Range& destination, Range&& source) -> void {
  destination.reserve(destination.size() + source.size());
  std::move(std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()), std::back_inserter(destination));
  source.clear();
}

template<typename Range>
concept copy_appendable = std::copyable<typename Range::value_type> && requires(Range& destination, const Range& source, std::size_t n) {
  source.size();
  destination.size();
  destination.reserve(n);
  destination.insert(destination.end(), source.begin(), source.end());
}; // concept copy_appendable

/**
 * @brief Appends elements from a source vector to a destination vector by copying them.
 *
 * @tparam Type A copyable type stored in the vector.
 * 
 * @param destination The vector to which elements will be appended.
 * @param source The vector from which elements will be copied. It remains unchanged.
 */
template<copy_appendable Range>
auto append(Range& destination, const Range& source) -> void {
  destination.reserve(destination.size() + source.size());
  std::copy(source.begin(), source.end(), std::back_inserter(destination));
}

/**
 * @brief Concept for a range descriptor that calculates a start/end subrange from a given size.
 */
template<typename Type>
concept range_descriptor = requires(const Type& descriptor, const std::size_t size) {
  { descriptor.is_valid(size) } -> std::same_as<bool>;
  { descriptor.start(size) } -> std::same_as<std::size_t>;
  { descriptor.end(size) } -> std::same_as<std::size_t>;
}; // concept range_descriptor

/**
 * @brief Range specified by an offset and a count.
 */
struct offset_count {

  std::size_t offset;
  std::size_t count;

  constexpr auto is_valid(const std::size_t size) const -> bool {
    return offset <= size;
  }

  constexpr auto start([[maybe_unused]] const std::size_t size) const -> std::size_t {
    return offset;
  }

  constexpr auto end(const std::size_t size) const -> std::size_t {
    return std::min(offset + count, size);
  }

}; // struct offset_count

/**
 * @brief Range specified by an explicit start and end index.
 */
struct start_end {

  std::size_t begin_index;
  std::size_t end_index;

  constexpr auto is_valid(const std::size_t size) const -> bool {
    return begin_index <= end_index && end_index <= size;
  }

  constexpr auto start([[maybe_unused]] const std::size_t size) const -> std::size_t {
    return begin_index;
  }

  constexpr auto end([[maybe_unused]] const std::size_t size) const -> std::size_t {
    return end_index;
  }

}; // struct start_end

/**
 * @brief Extracts a subrange of elements from the given vector by moving them into a new vector.
 *
 * @tparam Type A movable type stored in the vector.
 * @tparam Range A type satisfying the range_descriptor concept.
 *
 * @param vector The source vector, which will be cleared after moving the subrange.
 * @param range A descriptor specifying the subrange.
 *
 * @return A new vector with copied elements, or empty if the range is invalid.
 *
 * @note The source vector will be cleared after this operation.
 */
template<std::movable Type, range_descriptor Range>
auto subrange(std::vector<Type>&& vector, const Range& range) -> std::vector<Type> {
  const auto size = vector.size();

  if (!range.is_valid(size)) {
    return {};
  }

  const auto start = range.start(size);
  const auto end = range.end(size);

  auto result = std::vector<Type>{};
  result.reserve(end - start);

  std::move(std::make_move_iterator(vector.begin() + start), std::make_move_iterator(vector.begin() + end), std::back_inserter(result));

  vector.clear();

  return result;
}

/**
 * @brief Extracts a subrange of elements from the given vector by copying them into a new vector.
 *
 * @tparam Type A copyable type stored in the vector.
 * @tparam Range A type satisfying the range_descriptor concept.
 *
 * @param vector The source vector to copy elements from. It remains unchanged.
 * @param range A descriptor specifying the subrange.
 *
 * @return A new vector with moved elements, or empty if the range is invalid.
 */
template<std::copyable Type, range_descriptor Range>
auto subrange(const std::vector<Type>& vector, const Range& range) -> std::vector<Type> {
  const auto size = vector.size();

  if (!range.is_valid(size)) {
    return {};
  }

  const auto start = range.start(size);
  const auto end = range.end(size);

  auto result = std::vector<Type>{};
  result.reserve(end - start);

  std::copy(vector.begin() + start, vector.begin() + end, std::back_inserter(result));

  return result;
}

template<std::copyable Type>
auto make_vector(const std::size_t size, const Type& value = Type{}) -> std::vector<Type> {
  auto result = std::vector<Type>{};

  result.resize(size, value);

  return result;
}

template<typename Type>
auto make_reserved_vector(const std::size_t size) -> std::vector<Type> {
  auto result = std::vector<Type>{};

  result.reserve(size);

  return result;
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_ITERATOR_HPP_
