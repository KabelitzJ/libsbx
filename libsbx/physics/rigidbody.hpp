// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_RIGIDBODY_HPP_
#define LIBSBX_PHYSICS_RIGIDBODY_HPP_

#include <cstdint>

#include <libsbx/math/matrix3x3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>

namespace sbx::physics {

enum class body_type : std::uint8_t {
  dynamic_body,
  kinematic,
  static_body
}; // enum class body_type

/**
 * @brief A rigid body: mass, velocities and the accumulators/derived state the solver needs.
 * Attach alongside a shape_collider or mesh_collider for the body to actually collide with
 * anything — a rigidbody with neither still falls (or holds still, if static/kinematic) but never
 * generates contacts.
 */
struct rigidbody {
  body_type type{body_type::dynamic_body};

  std::float_t inverse_mass{1.0f};

  math::vector3 local_inverse_inertia{1.0f, 1.0f, 1.0f};

  math::vector3 linear_velocity{math::vector3::zero};
  math::vector3 angular_velocity{math::vector3::zero};

  std::float_t linear_damping{0.01f};
  std::float_t angular_damping{0.05f};
  std::float_t gravity_scale{1.0f};

  math::vector3 force_accumulator{math::vector3::zero};
  math::vector3 torque_accumulator{math::vector3::zero};
  math::matrix3x3 world_inverse_inertia{math::matrix3x3::identity};

  bool is_sleeping{false};
  std::float_t sleep_timer{0.0f};
}; // struct rigidbody

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_RIGIDBODY_HPP_
