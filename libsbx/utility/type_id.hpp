// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_UTILITY_TYPE_ID_HPP_
#define LIBSBX_UTILITY_TYPE_ID_HPP_

#include <cstdint>
#include <type_traits>

namespace sbx::utility {

namespace detail {

struct id_generator final {
  [[nodiscard]] static auto next() noexcept -> std::uint32_t {
    static auto id = std::uint32_t{};
    return id++;
  }
}; // struct id_generator

} // namespace detail

/**
 * @brief A scoped type ID generator. Allows for generating unique IDs for types within a specific scope.
 *
 * @tparam Scope The scope in which the type ID is generated.
 * @tparam Type The type for which the ID is generated.
 */
template<typename Type>
struct type_id {

  using type = Type;

  /**
   * @brief Generates a unique ID for the type.
   *
   * @return A unique ID for the type.
   */
  [[nodiscard]] static auto value() noexcept -> std::uint32_t {
    static const auto value = detail::id_generator::next();

    return value;
  }

  [[nodiscard]] constexpr operator std::uint32_t() const noexcept {
    return value();
  }

}; // struct type_id

} // namespace sbx::utility

#endif // LIBSBX_UTILITY_TYPE_ID_HPP_
