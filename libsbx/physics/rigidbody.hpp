// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_RIGIDBODY_HPP_
#define LIBSBX_PHYSICS_RIGIDBODY_HPP_

#include <cstdint>

#include <libsbx/math/matrix3x3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/node.hpp>

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

/**
 * @brief A node's rigidbody if it has one, or @p fallback otherwise -- for the handful of places
 * (narrowphase pair processing, the solver) that need *some* rigidbody& to read/write regardless of
 * whether this specific node was ever given one. A `shape_collider`/`mesh_collider` node with no
 * rigidbody anywhere in its ancestor chain is an implicit static collider (matching Unity: a
 * Collider alone, no Rigidbody, is a static one) -- nothing ever adds a real component for it, so
 * every unconditional get_component<rigidbody>() in physics goes through this instead. Safe to
 * write through the returned reference even when it's `fallback`: every write any caller makes is
 * scaled by effective_inverse_mass/effective_inverse_inertia (solver.cpp), which are already forced
 * to zero for anything but dynamic_body, so a write to a fallback body is a mathematical no-op and
 * is never read back afterward either way.
 *
 * @p node is taken by value (a node handle is just a registry pointer + entity id, cheap to copy) so
 * get_component() below resolves to its non-const overload -- matching the same
 * copy-for-mutable-access idiom solver.cpp's apply_positional_correction already uses.
 */
[[nodiscard]] inline auto effective_rigidbody(scenes::node& node, rigidbody& fallback) -> rigidbody& {
  return node.has_component<rigidbody>() ? node.get_component<rigidbody>() : fallback;
}

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_RIGIDBODY_HPP_
