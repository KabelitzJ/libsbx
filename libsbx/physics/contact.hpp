// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/contact.hpp
 *
 * @brief Narrowphase output and solver input: contact points between a body pair sharing one normal and combined material.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_CONTACT_HPP_
#define LIBSBX_PHYSICS_CONTACT_HPP_

#include <cstdint>

#include <libsbx/math/vector3.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/scenes/node.hpp>

#include <libsbx/utility/hash.hpp>

namespace sbx::physics {

inline constexpr auto max_manifold_points = std::size_t{4};

/** @brief One contact point; its impulse accumulators are warm-started from the previous step's matching point, or start at zero. */
struct contact_point {
  math::vector3 point{math::vector3::zero};        // world space
  std::float_t penetration_depth{0.0f};
  math::vector3 anchor_a{math::vector3::zero};      // point - node_a's position, recomputed every step
  math::vector3 anchor_b{math::vector3::zero};      // point - node_b's position, recomputed every step
  std::float_t normal_impulse{0.0f};
  std::float_t tangent_impulse_1{0.0f};
  std::float_t tangent_impulse_2{0.0f};
  std::uint32_t feature_id{0u};
}; // struct contact_point

/** @brief One colliding pair's narrowphase result: a world normal from A into B and up to @ref max_manifold_points points. */
struct contact_manifold {
  scenes::node node_a;
  scenes::node node_b;
  math::vector3 normal{math::vector3::up};
  std::float_t combined_friction{0.0f};
  std::float_t combined_restitution{0.0f};
  containers::static_vector<contact_point, max_manifold_points> points{};

  // Set when either node's own collider is a trigger (compound subtrees aren't checked, a v1 simplification). Triggers fire contact events but are excluded from the solver.
  bool is_trigger{false};
}; // struct contact_manifold

/** @brief Payload of on_contact_began/on_contact_ended. normal and point are only meaningful for began events, taken from the manifold's first point. */
struct collision_event {
  scenes::node node_a;
  scenes::node node_b;
  math::vector3 normal{math::vector3::up};
  math::vector3 point{math::vector3::zero};
  bool is_trigger{false};
}; // struct collision_event

/** @brief Identifies a pair for the warm-start cache regardless of A/B order; only construct it with @ref make_manifold_key, which canonicalizes the order. */
struct manifold_key {
  scenes::node node_a;
  scenes::node node_b;
}; // struct manifold_key

[[nodiscard]] inline auto make_manifold_key(const scenes::node& a, const scenes::node& b) -> manifold_key {
  return (a.id().value() < b.id().value()) ? manifold_key{a, b} : manifold_key{b, a};
}

[[nodiscard]] inline auto operator==(const manifold_key& lhs, const manifold_key& rhs) -> bool {
  return lhs.node_a == rhs.node_a && lhs.node_b == rhs.node_b;
}

} // namespace sbx::physics

template<>
struct std::hash<sbx::physics::manifold_key> {

  auto operator()(const sbx::physics::manifold_key& key) const noexcept -> std::size_t {
    auto seed = std::size_t{0};
    sbx::utility::hash_combine(seed, key.node_a, key.node_b);
    return seed;
  }

}; // struct std::hash<sbx::physics::manifold_key>

#endif // LIBSBX_PHYSICS_CONTACT_HPP_
