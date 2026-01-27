// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ENTITY_HPP_
#define LIBSBX_ENTITY_HPP_

#include <cinttypes>
#include <memory>
#include <type_traits>
#include <utility>

#include <libsbx/memory/concepts.hpp>

namespace sbx::ecs {

/**
 * @brief Primary template
 *
 * @tparam Type Type of the entity
 */
template<typename Type>
struct basic_entity_traits;

/** 
 * @brief Entity traits for 32 bit entity representation 
 */
template<>
struct basic_entity_traits<std::uint32_t> {
  using value_type = std::uint32_t;

  using entity_type = std::uint32_t;
  using version_type = std::uint16_t;

  inline static constexpr auto entity_mask = entity_type{0xFFFFF};
  inline static constexpr auto version_mask = entity_type{0xFFF};
}; // struct basic_entity_traits

/** 
 * @brief Entity traits for 64 bit entity representation 
 */
template<>
struct basic_entity_traits<std::uint64_t> {
  using value_type = std::uint64_t;

  using entity_type = std::uint64_t;
  using version_type = std::uint32_t;

  inline static constexpr auto entity_mask = entity_type{0xFFFFFFFF};
  inline static constexpr auto version_mask = entity_type{0xFFFFFFFF};
}; // struct basic_entity_traits

/**
 * @brief Entity traits for an enum entity representation. Propagates to the underlying type
 *
 * @tparam Type Enumeration type
 */
template<typename Type>
requires (std::is_enum_v<Type>)
struct basic_entity_traits<Type> : basic_entity_traits<std::underlying_type_t<Type>> {
  using value_type = Type;
}; // struct basic_entity_traits

template<typename Type>
requires (std::is_class_v<Type>)
struct basic_entity_traits<Type> : basic_entity_traits<typename Type::entity_type> {
  using value_type = Type;
}; // struct basic_entity_traits

/**
 * @brief Entity traits
 *
 * @tparam Type Unsigned integral, enumeration or class type
 */
template<typename Type>
struct entity_traits : basic_entity_traits<Type> {

  using value_type = basic_entity_traits<Type>::value_type;
  using entity_type = basic_entity_traits<Type>::entity_type;
  using version_type = basic_entity_traits<Type>::version_type;

  inline static constexpr auto entity_mask = basic_entity_traits<Type>::entity_mask;
  inline static constexpr auto version_mask = basic_entity_traits<Type>::version_mask;
  inline static constexpr auto version_shift = std::popcount(entity_mask);

  inline static constexpr auto page_size = std::size_t{4096};

  static constexpr auto to_integral(const value_type value) noexcept -> entity_type;

  static constexpr auto to_entity(const value_type value) noexcept -> entity_type;

  static constexpr auto to_version(const value_type value) noexcept -> version_type;

  static constexpr auto next(const value_type value) noexcept -> value_type;

  static constexpr auto construct(const entity_type entity, const version_type version = version_type{0}) noexcept -> value_type;

  static constexpr auto combine(const entity_type lhs, const entity_type rhs) noexcept -> value_type;

}; // struct entity_traits

enum class entity : std::uint32_t { };

struct basic_null_entity {

  template<typename Entity>
  [[nodiscard]] constexpr operator Entity() const noexcept;

  [[nodiscard]] constexpr auto operator==([[maybe_unused]] const basic_null_entity other) const noexcept -> bool;

  template<typename Entity>
  [[nodiscard]] constexpr bool operator==(const Entity entity) const noexcept;

}; // struct basic_null_entity

inline constexpr auto null_entity = basic_null_entity{};

struct basic_tombstone_entity {

  template<typename Entity>
  [[nodiscard]] constexpr operator Entity() const noexcept;

  [[nodiscard]] constexpr auto operator==([[maybe_unused]] const basic_tombstone_entity other) const noexcept -> bool;

  template<typename Entity>
  [[nodiscard]] constexpr bool operator==(const Entity entity) const noexcept;

}; // struct basic_tombstone_entity

inline constexpr auto tombstone_entity = basic_tombstone_entity{};

} // namespace sbx::ecs

#include <libsbx/ecs/entity.ipp>

#endif // LIBSBX_ENTITY_HPP_
