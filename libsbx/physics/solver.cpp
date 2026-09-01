// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/solver.hpp>

#include <algorithm>
#include <cmath>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/matrix3x3.hpp>
#include <libsbx/math/matrix_cast.hpp>
#include <libsbx/math/quaternion.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/physics/rigidbody.hpp>

namespace sbx::physics {

// A sleeping dynamic body is treated exactly like a static one here -- infinite mass, immovable --
// so anything resting on it solves correctly without ever perturbing its (frozen, exactly-zero)
// velocity. That in turn is what lets physics_module::_narrowphase() wake a sleeping body only on a
// genuinely moving contact instead of any contact at all: a still-sleeping neighbor just acts as
// solid ground for whatever's touching it, the same as the static floor would.
[[nodiscard]] auto effective_inverse_mass(const rigidbody& body) -> std::float_t {
  return (body.type == body_type::dynamic_body && !body.is_sleeping) ? body.inverse_mass : 0.0f;
}

[[nodiscard]] auto effective_inverse_inertia(const rigidbody& body) -> math::matrix3x3 {
  return (body.type == body_type::dynamic_body && !body.is_sleeping) ? body.world_inverse_inertia : math::matrix3x3::zero;
}

[[nodiscard]] auto point_velocity(const rigidbody& body, const math::vector3& anchor) -> math::vector3 {
  return body.linear_velocity + math::vector3::cross(body.angular_velocity, anchor);
}

// Below this closing speed, restitution is treated as zero -- kills the endless micro-bounce a
// resting body would otherwise pick up from float noise in the normal impulse.
inline constexpr auto restitution_velocity_threshold = std::float_t{1.0f};

auto integrate_forces(scenes::scene& scene, const math::vector3& gravity, std::float_t dt) -> void {
  for (auto&& [entity, body, local] : scene.query<rigidbody, scenes::local_transform>().each()) {
    if (body.type != body_type::dynamic_body || body.is_sleeping) {
      continue;
    }

    const auto rotation_matrix = math::matrix_cast<math::matrix3x3>(local.rotation);
    const auto local_inertia_matrix = math::matrix3x3{body.local_inverse_inertia.x(), body.local_inverse_inertia.y(), body.local_inverse_inertia.z()};

    body.world_inverse_inertia = rotation_matrix * local_inertia_matrix * math::matrix3x3::transposed(rotation_matrix);

    if (body.inverse_mass > 0.0f) {
      body.linear_velocity = body.linear_velocity + (gravity * body.gravity_scale + body.force_accumulator * body.inverse_mass) * dt;
    }

    body.angular_velocity = body.angular_velocity + (body.world_inverse_inertia * body.torque_accumulator) * dt;

    // Semi-implicit exponential damping.
    body.linear_velocity = body.linear_velocity * (1.0f / (1.0f + dt * body.linear_damping));
    body.angular_velocity = body.angular_velocity * (1.0f / (1.0f + dt * body.angular_damping));

    body.force_accumulator = math::vector3::zero;
    body.torque_accumulator = math::vector3::zero;
  }
}

auto prepare_velocity_constraints(std::vector<contact_manifold>& manifolds) -> std::vector<velocity_constraint> {
  auto constraints = std::vector<velocity_constraint>{};
  constraints.reserve(manifolds.size());

  for (auto& manifold : manifolds) {
    // Fresh per manifold, not shared/static: harmless even so (every write to a fallback is a
    // mathematical no-op, see rigidbody.hpp's effective_rigidbody doc comment), but this avoids any
    // aliasing question between two different implicit-static manifolds entirely.
    auto fallback_a = rigidbody{body_type::static_body};
    auto fallback_b = rigidbody{body_type::static_body};

    auto& body_a = effective_rigidbody(manifold.node_a, fallback_a);
    auto& body_b = effective_rigidbody(manifold.node_b, fallback_b);

    const auto inv_mass_a = effective_inverse_mass(body_a);
    const auto inv_mass_b = effective_inverse_mass(body_b);

    if (inv_mass_a <= 0.0f && inv_mass_b <= 0.0f) {
      continue; // both immovable -- nothing for the solver to do
    }

    const auto inv_inertia_a = effective_inverse_inertia(body_a);
    const auto inv_inertia_b = effective_inverse_inertia(body_b);

    auto constraint = velocity_constraint{};
    constraint.node_a = manifold.node_a;
    constraint.node_b = manifold.node_b;
    constraint.normal = manifold.normal;
    constraint.friction = manifold.combined_friction;
    constraint.restitution = manifold.combined_restitution;
    constraint.tangent_1 = math::vector3::normalized(math::vector3::orthogonal(manifold.normal));
    constraint.tangent_2 = math::vector3::cross(manifold.normal, constraint.tangent_1);

    const auto apply_impulse = [&](const math::vector3& impulse, const math::vector3& anchor_a, const math::vector3& anchor_b) {
      body_a.linear_velocity = body_a.linear_velocity - impulse * inv_mass_a;
      body_a.angular_velocity = body_a.angular_velocity - inv_inertia_a * math::vector3::cross(anchor_a, impulse);
      body_b.linear_velocity = body_b.linear_velocity + impulse * inv_mass_b;
      body_b.angular_velocity = body_b.angular_velocity + inv_inertia_b * math::vector3::cross(anchor_b, impulse);
    };

    for (auto& point : manifold.points) {
      auto constraint_point = velocity_constraint_point{};
      constraint_point.anchor_a = point.anchor_a;
      constraint_point.anchor_b = point.anchor_b;
      constraint_point.contact = &point;

      const auto compute_mass = [&](const math::vector3& direction) -> std::float_t {
        const auto ra_x_d = math::vector3::cross(point.anchor_a, direction);
        const auto rb_x_d = math::vector3::cross(point.anchor_b, direction);

        const auto k = inv_mass_a + inv_mass_b
          + math::vector3::dot(inv_inertia_a * ra_x_d, ra_x_d)
          + math::vector3::dot(inv_inertia_b * rb_x_d, rb_x_d);

        return (k > math::epsilonf) ? (1.0f / k) : 0.0f;
      };

      constraint_point.normal_mass = compute_mass(manifold.normal);
      constraint_point.tangent_mass_1 = compute_mass(constraint.tangent_1);
      constraint_point.tangent_mass_2 = compute_mass(constraint.tangent_2);

      const auto relative_velocity = point_velocity(body_b, point.anchor_b) - point_velocity(body_a, point.anchor_a);
      const auto closing_speed = math::vector3::dot(relative_velocity, manifold.normal);

      constraint_point.velocity_bias = (closing_speed < -restitution_velocity_threshold) ? (-constraint.restitution * closing_speed) : 0.0f;

      // Warm start: point.{normal,tangent_1,tangent_2}_impulse is either still zero (a point with
      // no match in the previous step's cached manifold) or was just seeded by
      // physics_module::_warm_start_manifolds from the matching point last step. Either way, carry
      // it into the constraint and apply it once now, before the caller's iterative solve even
      // starts -- that's what lets a resting stack's supporting impulse persist instead of being
      // rebuilt from zero every step.
      constraint_point.normal_impulse = point.normal_impulse;
      constraint_point.tangent_impulse_1 = point.tangent_impulse_1;
      constraint_point.tangent_impulse_2 = point.tangent_impulse_2;

      const auto warm_impulse = constraint.normal * constraint_point.normal_impulse
        + constraint.tangent_1 * constraint_point.tangent_impulse_1
        + constraint.tangent_2 * constraint_point.tangent_impulse_2;

      apply_impulse(warm_impulse, point.anchor_a, point.anchor_b);

      constraint.points.push_back(constraint_point);
    }

    constraints.push_back(constraint);
  }

  return constraints;
}

auto solve_velocity_constraints(std::vector<velocity_constraint>& constraints, std::uint32_t iterations) -> void {
  for (auto iteration = std::uint32_t{0}; iteration < iterations; ++iteration) {
    for (auto& constraint : constraints) {
      auto fallback_a = rigidbody{body_type::static_body};
      auto fallback_b = rigidbody{body_type::static_body};

      auto& body_a = effective_rigidbody(constraint.node_a, fallback_a);
      auto& body_b = effective_rigidbody(constraint.node_b, fallback_b);

      const auto inv_mass_a = effective_inverse_mass(body_a);
      const auto inv_mass_b = effective_inverse_mass(body_b);
      const auto inv_inertia_a = effective_inverse_inertia(body_a);
      const auto inv_inertia_b = effective_inverse_inertia(body_b);

      const auto apply_impulse = [&](const math::vector3& impulse, const math::vector3& anchor_a, const math::vector3& anchor_b) {
        body_a.linear_velocity = body_a.linear_velocity - impulse * inv_mass_a;
        body_a.angular_velocity = body_a.angular_velocity - inv_inertia_a * math::vector3::cross(anchor_a, impulse);
        body_b.linear_velocity = body_b.linear_velocity + impulse * inv_mass_b;
        body_b.angular_velocity = body_b.angular_velocity + inv_inertia_b * math::vector3::cross(anchor_b, impulse);
      };

      for (auto& point : constraint.points) {
        // Normal impulse, clamped non-negative (a contact can only push, never pull).
        {
          const auto relative_velocity = point_velocity(body_b, point.anchor_b) - point_velocity(body_a, point.anchor_a);
          const auto vn = math::vector3::dot(relative_velocity, constraint.normal);

          const auto new_impulse = std::max(point.normal_impulse + point.normal_mass * (-vn + point.velocity_bias), 0.0f);
          const auto delta = new_impulse - point.normal_impulse;
          point.normal_impulse = new_impulse;

          apply_impulse(constraint.normal * delta, point.anchor_a, point.anchor_b);
        }

        // Friction: two tangent impulses, each Coulomb-clamped to the current normal impulse.
        const auto friction_limit = constraint.friction * point.normal_impulse;

        {
          const auto relative_velocity = point_velocity(body_b, point.anchor_b) - point_velocity(body_a, point.anchor_a);
          const auto vt = math::vector3::dot(relative_velocity, constraint.tangent_1);

          const auto new_impulse = std::clamp(point.tangent_impulse_1 - vt * point.tangent_mass_1, -friction_limit, friction_limit);
          const auto delta = new_impulse - point.tangent_impulse_1;
          point.tangent_impulse_1 = new_impulse;

          apply_impulse(constraint.tangent_1 * delta, point.anchor_a, point.anchor_b);
        }

        {
          const auto relative_velocity = point_velocity(body_b, point.anchor_b) - point_velocity(body_a, point.anchor_a);
          const auto vt = math::vector3::dot(relative_velocity, constraint.tangent_2);

          const auto new_impulse = std::clamp(point.tangent_impulse_2 - vt * point.tangent_mass_2, -friction_limit, friction_limit);
          const auto delta = new_impulse - point.tangent_impulse_2;
          point.tangent_impulse_2 = new_impulse;

          apply_impulse(constraint.tangent_2 * delta, point.anchor_a, point.anchor_b);
        }
      }
    }
  }
}

auto store_impulses(std::vector<velocity_constraint>& constraints) -> void {
  for (auto& constraint : constraints) {
    for (auto& point : constraint.points) {
      if (point.contact == nullptr) {
        continue;
      }

      point.contact->normal_impulse = point.normal_impulse;
      point.contact->tangent_impulse_1 = point.tangent_impulse_1;
      point.contact->tangent_impulse_2 = point.tangent_impulse_2;
    }
  }
}

auto integrate_velocities(scenes::scene& scene, std::float_t dt) -> void {
  for (auto&& [entity, body, local] : scene.query<rigidbody, scenes::local_transform>().each()) {
    if (body.type == body_type::static_body || (body.type == body_type::dynamic_body && body.is_sleeping)) {
      continue;
    }

    local.position = local.position + body.linear_velocity * dt;

    const auto angular_speed = body.angular_velocity.length();

    if (angular_speed > math::epsilonf) {
      const auto axis = body.angular_velocity * (1.0f / angular_speed);
      const auto delta = math::quaternion{axis, math::angle{math::radian{angular_speed * dt}}};

      local.rotation = math::quaternion::normalized(delta * local.rotation);
    }
  }
}

auto apply_positional_correction(std::vector<contact_manifold>& manifolds, std::float_t percent, std::float_t slop) -> void {
  for (const auto& manifold : manifolds) {
    if (manifold.points.is_empty()) {
      continue;
    }

    // Copy the node handles so get_component() resolves to its non-const overload -- manifold
    // itself is only ever read here, but the node handles it carries must stay mutable.
    auto node_a = manifold.node_a;
    auto node_b = manifold.node_b;

    auto fallback_a = rigidbody{body_type::static_body};
    auto fallback_b = rigidbody{body_type::static_body};

    auto& body_a = effective_rigidbody(node_a, fallback_a);
    auto& body_b = effective_rigidbody(node_b, fallback_b);

    const auto inv_mass_a = effective_inverse_mass(body_a);
    const auto inv_mass_b = effective_inverse_mass(body_b);
    const auto total_inverse_mass = inv_mass_a + inv_mass_b;

    if (total_inverse_mass <= 0.0f) {
      continue;
    }

    auto max_depth = 0.0f;

    for (const auto& point : manifold.points) {
      max_depth = std::max(max_depth, point.penetration_depth);
    }

    const auto correction_magnitude = std::max(max_depth - slop, 0.0f) / total_inverse_mass * percent;

    if (correction_magnitude <= 0.0f) {
      continue;
    }

    const auto correction = manifold.normal * correction_magnitude;

    auto& local_a = node_a.get_component<scenes::local_transform>();
    auto& local_b = node_b.get_component<scenes::local_transform>();

    local_a.position = local_a.position - correction * inv_mass_a;
    local_b.position = local_b.position + correction * inv_mass_b;
  }
}

auto update_sleep_timers(scenes::scene& scene, std::float_t dt, std::float_t linear_threshold, std::float_t angular_threshold, std::float_t time_to_sleep) -> void {
  for (auto&& [entity, body] : scene.query<rigidbody>().each()) {
    if (body.type != body_type::dynamic_body) {
      continue;
    }

    const auto is_slow = body.linear_velocity.length_squared() < linear_threshold * linear_threshold
      && body.angular_velocity.length_squared() < angular_threshold * angular_threshold;

    if (!is_slow) {
      body.sleep_timer = 0.0f;
      body.is_sleeping = false;
      continue;
    }

    body.sleep_timer += dt;

    if (body.sleep_timer >= time_to_sleep) {
      body.is_sleeping = true;
      body.linear_velocity = math::vector3::zero;
      body.angular_velocity = math::vector3::zero;
    }
  }
}

} // namespace sbx::physics
