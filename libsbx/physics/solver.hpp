// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/solver.hpp
 *
 * @brief Force and velocity integration, the sequential impulse (PGS) velocity solver, positional (NGS) correction and sleeping; stateless functions over the scene and this step's manifolds.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_SOLVER_HPP_
#define LIBSBX_PHYSICS_SOLVER_HPP_

#include <cstdint>
#include <span>
#include <vector>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/math/vector3.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>

#include <libsbx/physics/contact.hpp>
#include <libsbx/physics/rigidbody.hpp>

namespace sbx::physics {

struct velocity_constraint_point {
  math::vector3 anchor_a{math::vector3::zero};
  math::vector3 anchor_b{math::vector3::zero};
  std::float_t normal_mass{0.0f};
  std::float_t tangent_mass_1{0.0f};
  std::float_t tangent_mass_2{0.0f};
  std::float_t velocity_bias{0.0f}; // restitution term
  std::float_t normal_impulse{0.0f};
  std::float_t tangent_impulse_1{0.0f};
  std::float_t tangent_impulse_2{0.0f};
  memory::observer_ptr<contact_point> contact{nullptr}; // the source manifold point, for store_impulses
}; // struct velocity_constraint_point

struct velocity_constraint {
  scenes::node node_a;
  scenes::node node_b;
  // Resolved once in prepare_velocity_constraints, so the solver iterations don't go through the ECS.
  memory::observer_ptr<rigidbody> body_a{nullptr};
  memory::observer_ptr<rigidbody> body_b{nullptr};
  math::vector3 normal{math::vector3::up};
  math::vector3 tangent_1{math::vector3::right};
  math::vector3 tangent_2{math::vector3::forward};
  std::float_t friction{0.0f};
  std::float_t restitution{0.0f};
  containers::static_vector<velocity_constraint_point, max_manifold_points> points{};
}; // struct velocity_constraint

/**
 * @brief Applies gravity, force and torque accumulators and damping to every awake dynamic body, then clears the accumulators and refreshes world_inverse_inertia.
 *
 * @param scene The scene.
 * @param gravity The gravity.
 * @param dt The step.
 */
auto integrate_forces(scenes::scene& scene, const math::vector3& gravity, std::float_t dt) -> void;

/**
 * @brief Builds one velocity constraint per manifold (skipping two immovable bodies) and applies the warm-started impulses once.
 *
 * Constraint points keep pointers into @p manifolds for store_impulses; a span lets the caller solve only the non-trigger prefix in place.
 *
 * @param manifolds The manifolds to solve.
 *
 * @return The constraints.
 */
[[nodiscard]] auto prepare_velocity_constraints(std::span<contact_manifold> manifolds) -> std::vector<velocity_constraint>;

/**
 * @brief Runs projected Gauss-Seidel passes: a clamped normal impulse per point, then two Coulomb-clamped tangent impulses.
 *
 * @param constraints The constraints.
 * @param iterations The number of passes.
 */
auto solve_velocity_constraints(std::vector<velocity_constraint>& constraints, std::uint32_t iterations) -> void;

/**
 * @brief Writes the final impulses back into the manifolds' contact points for next step's warm start.
 *
 * @param constraints The solved constraints.
 */
auto store_impulses(std::vector<velocity_constraint>& constraints) -> void;

/**
 * @brief Semi-implicit Euler position and rotation integration for every moving, awake body.
 *
 * @param scene The scene.
 * @param dt The step.
 */
auto integrate_velocities(scenes::scene& scene, std::float_t dt) -> void;

/**
 * @brief NGS positional correction: pushes bodies apart by `percent` of the penetration beyond `slop`, split by inverse mass. Translation only.
 *
 * @param manifolds The manifolds, excluding triggers.
 * @param percent The fraction of the penetration to correct.
 * @param slop The allowed penetration.
 */
auto apply_positional_correction(std::span<contact_manifold> manifolds, std::float_t percent, std::float_t slop) -> void;

/**
 * @brief Advances or resets each dynamic body's sleep timer by its velocities and puts it to sleep (zeroing them) after time_to_sleep.
 *
 * @param scene The scene.
 * @param dt The step.
 * @param linear_threshold The linear speed below which a body counts as resting.
 * @param angular_threshold The angular speed below which a body counts as resting.
 * @param time_to_sleep How long a body must rest before sleeping.
 */
auto update_sleep_timers(scenes::scene& scene, std::float_t dt, std::float_t linear_threshold, std::float_t angular_threshold, std::float_t time_to_sleep) -> void;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_SOLVER_HPP_
