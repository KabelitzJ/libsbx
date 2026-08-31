// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/contact.hpp
 *
 * @brief The output of narrowphase and the input to the solver: one or more contact points between
 * a pair of bodies, sharing a single collision normal and combined material properties.
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

/**
 * @brief One contact point. The impulse accumulator fields are seeded from the previous step's
 * matching point (see physics_module::_warm_start_manifolds) and read/written by the solver each
 * step; a point with no previous-step match starts cold at zero.
 */
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

/**
 * @brief A narrowphase result for one colliding body pair: a shared world-space normal (pointing
 * from A into B) and up to @ref max_manifold_points contact points.
 */
struct contact_manifold {
  scenes::node node_a;
  scenes::node node_b;
  math::vector3 normal{math::vector3::up};
  std::float_t combined_friction{0.0f};
  std::float_t combined_restitution{0.0f};
  containers::static_vector<contact_point, max_manifold_points> points{};
}; // struct contact_manifold

/**
 * @brief Identifies a colliding pair for the warm-start manifold cache, independent of which side
 * narrowphase happened to call "A" and which "B" this step. Only ever construct through
 * @ref make_manifold_key -- its equality/hash are order-sensitive, and that's the function that
 * puts the two nodes into a canonical order.
 */
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
