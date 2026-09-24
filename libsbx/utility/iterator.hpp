// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_ITERATOR_HPP_
#define LIBSBX_UTILITY_ITERATOR_HPP_

#include <concepts>
#include <cstddef>
#include <vector>

namespace sbx::utility {

/**
 * @brief Bundles the standard iterator member typedefs (iterator_category, value_type,
 * etc.) for a custom iterator type to inherit from.
 *
 * @tparam Category The iterator category tag (e.g. std::forward_iterator_tag).
 * @tparam Type The value type.
 * @tparam Distance The difference type. Defaults to std::ptrdiff_t.
 * @tparam Pointer The pointer type. Defaults to Type*.
 * @tparam Reference The reference type. Defaults to Type&.
 */
template<typename Category, typename Type, typename Distance = std::ptrdiff_t, typename Pointer = Type*, typename Reference = Type&>
struct iterator {
  using iterator_category = Category;
  using value_type = Type;
  using difference_type = Distance;
  using pointer = Pointer;
  using reference = Reference;
}; // struct iterator

/**
 * @brief Builds a vector of size elements, each copy-constructed from value.
 *
 * @tparam Type The element type; must be copyable.
 *
 * @param size The number of elements.
 * @param value The value to copy into each element.
 *
 * @return The resulting vector.
 */
template<std::copyable Type>
auto make_vector(const std::size_t size, const Type& value = Type{}) -> std::vector<Type> {
  auto result = std::vector<Type>{};

  result.resize(size, value);

  return result;
}

/**
 * @brief Builds an empty vector with capacity already reserved for size elements.
 *
 * @tparam Type The element type.
 *
 * @param size The capacity to reserve.
 *
 * @return The resulting vector.
 */
template<typename Type>
auto make_reserved_vector(const std::size_t size) -> std::vector<Type> {
  auto result = std::vector<Type>{};

  result.reserve(size);

  return result;
}

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_ITERATOR_HPP_
