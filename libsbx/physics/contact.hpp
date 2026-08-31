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

namespace sbx::physics {

inline constexpr auto max_manifold_points = std::size_t{4};

/**
 * @brief One contact point. The impulse accumulator fields are provisioned for a future
 * warm-starting pass (persisting/matching manifolds frame-to-frame) but unused by the v1 solver,
 * which starts every point cold each step.
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

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_CONTACT_HPP_
