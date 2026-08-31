// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/solver.hpp
 *
 * @brief Force/velocity integration and the sequential-impulse (PGS) velocity solver, plus
 * positional (NGS) correction and sleep bookkeeping. Free functions operating on a scene and the
 * manifolds/constraints physics_module builds each fixed step -- no state of their own.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_SOLVER_HPP_
#define LIBSBX_PHYSICS_SOLVER_HPP_

#include <cstdint>
#include <vector>

#include <libsbx/math/vector3.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>

#include <libsbx/physics/contact.hpp>

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
}; // struct velocity_constraint_point

struct velocity_constraint {
  scenes::node node_a;
  scenes::node node_b;
  math::vector3 normal{math::vector3::up};
  math::vector3 tangent_1{math::vector3::right};
  math::vector3 tangent_2{math::vector3::forward};
  std::float_t friction{0.0f};
  std::float_t restitution{0.0f};
  containers::static_vector<velocity_constraint_point, max_manifold_points> points{};
}; // struct velocity_constraint

/**
 * @brief Applies gravity/force/torque accumulators and damping to every awake dynamic body, then
 * clears the accumulators and refreshes rigidbody::world_inverse_inertia from the body's current
 * rotation. Static/kinematic and sleeping bodies are untouched.
 */
auto integrate_forces(scenes::scene& scene, const math::vector3& gravity, std::float_t dt) -> void;

/**
 * @brief Builds one velocity_constraint per manifold (effective mass terms, tangent basis,
 * restitution bias), skipping manifolds between two immovable bodies.
 */
[[nodiscard]] auto prepare_velocity_constraints(const std::vector<contact_manifold>& manifolds) -> std::vector<velocity_constraint>;

/**
 * @brief Runs @p iterations passes of sequential-impulse (projected Gauss-Seidel) resolution over
 * @p constraints: a clamped normal impulse per point, then two Coulomb-clamped tangent impulses.
 */
auto solve_velocity_constraints(std::vector<velocity_constraint>& constraints, std::uint32_t iterations) -> void;

/**
 * @brief Semi-implicit Euler position/rotation integration for every non-static, non-sleeping body.
 */
auto integrate_velocities(scenes::scene& scene, std::float_t dt) -> void;

/**
 * @brief Non-linear Gauss-Seidel positional correction: nudges each manifold's bodies apart along
 * its normal by `percent` of the remaining penetration beyond `slop`, split by inverse-mass ratio.
 * Translation only -- no angular correction in v1.
 */
auto apply_positional_correction(std::vector<contact_manifold>& manifolds, std::float_t percent, std::float_t slop) -> void;

/**
 * @brief Advances (or resets) each dynamic body's sleep_timer based on whether its velocities are
 * below the given thresholds, and puts it to sleep (zeroing its velocities) once time_to_sleep is reached.
 */
auto update_sleep_timers(scenes::scene& scene, std::float_t dt, std::float_t linear_threshold, std::float_t angular_threshold, std::float_t time_to_sleep) -> void;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_SOLVER_HPP_
