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

/** @brief Entity traits for 32 bit entity representation */
template<>
struct basic_entity_traits<std::uint32_t> {
  using value_type = std::uint32_t;

  using entity_type = std::uint32_t;
  using version_type = std::uint16_t;

  inline static constexpr auto entity_mask = entity_type{0xFFFFF};
  inline static constexpr auto version_mask = entity_type{0xFFF};
};

/** @brief Entity traits for 64 bit entity representation */
template<>
struct basic_entity_traits<std::uint64_t> {
  using value_type = std::uint64_t;

  using entity_type = std::uint64_t;
  using version_type = std::uint32_t;

  inline static constexpr auto entity_mask = entity_type{0xFFFFFFFF};
  inline static constexpr auto version_mask = entity_type{0xFFFFFFFF};
};

/**
 * @brief Entity traits for an enum entity representation. Propagates to the underlying type
 *
 * @tparam Type Enumeration type
 */
template<typename Type>
requires (std::is_enum_v<Type>)
struct basic_entity_traits<Type> : basic_entity_traits<std::underlying_type_t<Type>> {
  using value_type = Type;
};

template<typename Type>
requires (std::is_class_v<Type>)
struct basic_entity_traits<Type> : basic_entity_traits<typename Type::entity_type> {
  using value_type = Type;
};

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

  /**
   * @brief Converts the type to its underlying type
   *
   * @param value
   *
   * @return
   */
  static constexpr auto to_integral(const value_type value) noexcept -> entity_type {
    return static_cast<entity_type>(value);
  }

  /**
   * @brief Gets the id part of the entity
   *
   * @param value
   *
   * @return
   */
  static constexpr auto to_entity(const value_type value) noexcept -> entity_type {
    return (to_integral(value) & entity_mask);
  }

  /**
   * @brief Gets the version part of the entity
   *
   * @param value
   *
   * @return
   */
  static constexpr auto to_version(const value_type value) noexcept -> version_type {
    return static_cast<version_type>(to_integral(value) >> version_shift) & version_mask;
  }

  static constexpr auto next(const value_type value) noexcept -> value_type {
    const auto version = to_version(value) + 1u;
    return construct(to_integral(value), static_cast<version_type>(version + (version == version_mask)));
  }

  /**
   * @brief Constructs a new entity form an id and a version
   *
   * @param id The id part of the entity
   * @param version The version part of the entity
   *
   * @return
   */
  static constexpr auto construct(const entity_type entity, const version_type version = version_type{0}) noexcept -> value_type {
    return value_type{(entity & entity_mask) | (static_cast<entity_type>(version & version_mask) << version_shift)};
  }

  static constexpr auto combine(const entity_type lhs, const entity_type rhs) noexcept -> value_type {
    return value_type{(lhs & entity_mask) | (rhs & (version_mask << version_shift))};
  }

}; // struct entity_traits

enum class entity : std::uint32_t { };

struct basic_null_entity {

  template<typename Entity>
  [[nodiscard]] constexpr operator Entity() const noexcept {
    return entity_traits<Entity>::construct(entity_traits<Entity>::entity_mask, entity_traits<Entity>::version_mask);
  }

  [[nodiscard]] constexpr auto operator==([[maybe_unused]] const basic_null_entity other) const noexcept -> bool {
    return true;
  }

  template<typename Entity>
  [[nodiscard]] constexpr bool operator==(const Entity entity) const noexcept {
    return entity_traits<Entity>::to_entity(entity) == entity_traits<Entity>::to_entity(*this);
  }

}; // struct basic_null_entity

inline constexpr auto null_entity = basic_null_entity{};

struct basic_tombstone_entity {

  template<typename Entity>
  [[nodiscard]] constexpr operator Entity() const noexcept {
    return entity_traits<Entity>::construct(entity_traits<Entity>::entity_mask, entity_traits<Entity>::version_mask);
  }

  [[nodiscard]] constexpr auto operator==([[maybe_unused]] const basic_tombstone_entity other) const noexcept -> bool {
    return true;
  }

  template<typename Entity>
  [[nodiscard]] constexpr bool operator==(const Entity entity) const noexcept {
    if constexpr (entity_traits<Entity>::version_mask == 0u) {
      return false;
    } else {
      return (entity_traits<Entity>::to_version(entity) == entity_traits<Entity>::to_version(*this));
    }
  }

}; // struct basic_tombstone_entity

inline constexpr auto tombstone_entity = basic_tombstone_entity{};

} // namespace sbx::ecs

#endif // LIBSBX_ENTITY_HPP_
