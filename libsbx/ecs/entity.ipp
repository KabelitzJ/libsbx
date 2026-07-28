// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/ecs/entity.hpp>

namespace sbx::ecs {

template<typename Type>
constexpr auto entity_traits<Type>::to_integral(const value_type value) noexcept -> entity_type {
  return static_cast<entity_type>(value);
}

template<typename Type>
constexpr auto entity_traits<Type>::to_entity(const value_type value) noexcept -> entity_type {
  return (to_integral(value) & entity_mask);
}

template<typename Type>
constexpr auto entity_traits<Type>::to_version(const value_type value) noexcept -> version_type {
  return static_cast<version_type>(to_integral(value) >> version_shift) & version_mask;
}

template<typename Type>
constexpr auto entity_traits<Type>::next(const value_type value) noexcept -> value_type {
  const auto version = to_version(value) + 1u;
  return construct(to_integral(value), static_cast<version_type>(version + (version == version_mask)));
}

template<typename Type>
constexpr auto entity_traits<Type>::construct(const entity_type entity, const version_type version) noexcept -> value_type {
  return value_type{(entity & entity_mask) | (static_cast<entity_type>(version & version_mask) << version_shift)};
}

template<typename Type>
constexpr auto entity_traits<Type>::combine(const entity_type lhs, const entity_type rhs) noexcept -> value_type {
  return value_type{(lhs & entity_mask) | (rhs & (version_mask << version_shift))};
}

template<typename Entity>
constexpr null_entity_t::operator Entity() const noexcept {
  return entity_traits<Entity>::construct(entity_traits<Entity>::entity_mask, entity_traits<Entity>::version_mask);
}

constexpr auto null_entity_t::operator==([[maybe_unused]] const null_entity_t other) const noexcept -> bool {
  return true;
}

template<typename Entity>
constexpr bool null_entity_t::operator==(const Entity entity) const noexcept {
  return entity_traits<Entity>::to_entity(entity) == entity_traits<Entity>::to_entity(*this);
}

template<typename Entity>
constexpr tombstone_entity_t::operator Entity() const noexcept {
  return entity_traits<Entity>::construct(entity_traits<Entity>::entity_mask, entity_traits<Entity>::version_mask);
}

constexpr auto tombstone_entity_t::operator==([[maybe_unused]] const tombstone_entity_t other) const noexcept -> bool {
  return true;
}

template<typename Entity>
constexpr bool tombstone_entity_t::operator==(const Entity entity) const noexcept {
  if constexpr (entity_traits<Entity>::version_mask == 0u) {
    return false;
  } else {
    return (entity_traits<Entity>::to_version(entity) == entity_traits<Entity>::to_version(*this));
  }
}

} // namespace sbx::ecs
