// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_RIGIDBODY_HPP_
#define LIBSBX_PHYSICS_RIGIDBODY_HPP_

#include <cstdint>

#include <libsbx/math/matrix3x3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/reflection/annotations.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/scenes/node.hpp>

namespace sbx::physics {

enum class [[=reflection::named]] body_type : std::uint8_t {
  dynamic_body,
  kinematic,
  static_body
}; // enum class body_type

/** @brief A rigid body: mass, velocities and the solver's accumulators and derived state. It only collides together with a shape_collider or mesh_collider. */
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
 * @brief The node's rigidbody, or @p fallback for implicit static colliders, which never get a real component.
 *
 * Writing through a fallback is safe: every write is scaled by effective inverse mass/inertia, which are zero for non-dynamic bodies, and fallbacks are never read back.
 *
 * @param node The node.
 * @param fallback The stand-in for nodes without a rigidbody.
 *
 * @return The rigidbody to use.
 */
[[nodiscard]] inline auto effective_rigidbody(scenes::node& node, rigidbody& fallback) -> rigidbody& {
  auto component = node.try_get_component<rigidbody>();
  return component ? *component : fallback;
}

/**
 * @brief Pointer form of effective_rigidbody() for callers that hold the result, backed by one thread_local fallback (safe for the same reason).
 *
 * @param node The node.
 *
 * @return The rigidbody to use.
 */
[[nodiscard]] inline auto effective_rigidbody_ptr(scenes::node& node) -> memory::observer_ptr<rigidbody> {
  static thread_local auto shared_fallback = rigidbody{body_type::static_body};
  auto component = node.try_get_component<rigidbody>();
  return component ? component : memory::observer_ptr<rigidbody>{&shared_fallback};
}

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_RIGIDBODY_HPP_
