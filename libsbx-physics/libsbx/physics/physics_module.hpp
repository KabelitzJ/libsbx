// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
#define LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_

#include <cmath>
#include <optional>
#include <vector>
#include <algorithm>
#include <ranges>
#include <unordered_map> // Added for unordered_map

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/module.hpp>

#include <libsbx/containers/octree.hpp>

#include <libsbx/math/constants.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/matrix_cast.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scenes/components/global_transform.hpp>
#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/relationship.hpp>

#include <libsbx/physics/shape_collider.hpp>
#include <libsbx/physics/collision_detection.hpp>
#include <libsbx/physics/rigidbody.hpp>

namespace sbx::physics {

class physics_module : public core::module<physics_module> {

  inline static const auto is_registered = register_module(stage::fixed, dependencies<scenes::scenes_module>{});

public:

  physics_module() = default;

  ~physics_module() override = default;

  auto update() -> void override {
    SBX_PROFILE_SCOPE("physics_module::update");
    SBX_MEMORY_SCOPE(memory::allocation_category::physics);

    const auto dt = core::engine::fixed_delta_time();

    _integrate_velocities(dt);

    const auto broad_phase_pairs = _collision_broad_phase();
    const auto collisions = _collision_narrow_phase(broad_phase_pairs);

    _resolve_collisions(collisions, dt);

    _integrate_positions(dt);

    _positional_correction(collisions);

    _update_character_controllers();
  }

private:

  using octree_type = containers::octree<scenes::node, 16u, 8u>;
  using collision_pair_type = typename octree_type::intersection;

  struct collision {
    scenes::node node_a;
    scenes::node node_b;
    collision_manifold manifold;
  }; // struct collision

  struct contact_constraint {
    math::vector3 normal;
    math::vector3 tangent;
    math::vector3 r_a;
    math::vector3 r_b;
    std::float_t normal_mass{0.0f};
    std::float_t tangent_mass{0.0f};
    std::float_t bias{0.0f};
    std::float_t normal_impulse{0.0f};
    std::float_t tangent_impulse{0.0f};
    std::uint64_t last_seen{0};
  }; // struct contact_constraint

  struct solver_contact {
    scenes::node node_a{};
    scenes::node node_b{};
    std::size_t key{};
    contact_constraint constraint{};
  }; // struct solver_contact

  auto _integrate_velocities(std::float_t dt) -> void {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto query = scene.query<scenes::transform, physics::rigidbody>();

    for (auto&& [node, transform, rigidbody] : query.each()) {
      if (rigidbody.is_static()) {
        continue;
      }

      rigidbody.update_inertia_tensor_world(math::matrix_cast<math::matrix3x3>(transform.rotation()));

      const auto linear_acceleration = (rigidbody.dynamic_forces() + rigidbody.constant_forces()) * rigidbody.inverse_mass();

      rigidbody.add_velocity(linear_acceleration * dt);
      rigidbody.set_velocity(rigidbody.velocity() * std::pow(1.0f - rigidbody.linear_damping(), dt));

      const auto angular_acceleration = rigidbody.inverse_inertia_tensor_world() * rigidbody.torque();

      rigidbody.add_angular_velocity(angular_acceleration * dt);
      rigidbody.set_angular_velocity(rigidbody.angular_velocity() * std::pow(1.0f - rigidbody.angular_damping(), dt));

      rigidbody.clear_dynamic_forces();
      rigidbody.clear_torque();
    }
  }

  auto _integrate_positions(std::float_t dt) -> void {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto query = scene.query<scenes::transform, physics::rigidbody>();

    for (auto&& [node, transform, rigidbody] : query.each()) {
      if (rigidbody.is_static()) {
        continue;
      }

      transform.position() += rigidbody.velocity() * dt;

      const auto angular_velocity = rigidbody.angular_velocity();

      if (angular_velocity.length_squared() > 0.0f) {
        const auto delta_rotation = math::quaternion{angular_velocity, 0.0f} * transform.rotation();
        transform.set_rotation(math::quaternion::normalized(transform.rotation() + delta_rotation * (0.5f * dt)));
      }

      rigidbody.update_inertia_tensor_world(math::matrix_cast<math::matrix3x3>(transform.rotation()));

      transform.bump_version();
    }
  }

  auto _collision_broad_phase() -> std::vector<collision_pair_type> {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto tree = octree_type{math::volume{math::vector3{-500.0f}, math::vector3{500.0f}}, 64u};
    auto query = scene.query<const physics::shape_collider, const physics::rigidbody>();

    for (auto&& [node, collider, rigidbody] : query.each()) {
      const auto translation = math::matrix4x4::translated(sbx::math::matrix4x4::identity, scene.world_position(node));
      const auto rotation = math::matrix_cast<math::matrix4x4>(scene.world_rotation(node));

      const auto volume = get_bounding_volume(collider, translation * rotation);

      tree.insert(node, volume);
    }

    return tree.intersections();
  }

  auto _collision_narrow_phase(const std::vector<collision_pair_type>& pairs) -> std::vector<collision> {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto collisions = std::vector<collision>{};
    collisions.reserve(pairs.size());

    for (auto [first, second] : pairs) {
      const auto node_a = std::min(first, second);
      const auto node_b = std::max(first, second);

      const auto& body_a = scene.get_component<physics::rigidbody>(node_a);
      const auto& body_b = scene.get_component<physics::rigidbody>(node_b);

      if (body_a.is_static() && body_b.is_static()) {
        continue;
      }

      const auto& collider_a = scene.get_component<physics::shape_collider>(node_a);
      const auto& collider_b = scene.get_component<physics::shape_collider>(node_b);

      const auto data_a = collider_data{scene.world_position(node_a), math::quaternion::normalized(scene.world_rotation(node_a)), collider_a};
      const auto data_b = collider_data{scene.world_position(node_b), math::quaternion::normalized(scene.world_rotation(node_b)), collider_b};

      if (auto manifold = check_collision(data_a, data_b); manifold) {
        for (const auto& point : manifold->contact_points) {
          scenes_module.add_debug_sphere(point, 0.2f, math::color::red(), 16u);
        }

        collisions.push_back({node_a, node_b, std::move(*manifold)});
      }
    }
  
    return collisions;
  }

  auto _resolve_collisions(const std::vector<collision>& collisions, std::float_t dt) -> void {
    SBX_PROFILE_SCOPE("physics_module::_resolve_collisions");

    auto& scene = core::engine::get_module<scenes::scenes_module>().scene();

    ++_solver_tick;

    constexpr auto solver_iterations = 32u;

    constexpr auto baumgarte_beta = 0.2f;       // 0.1 - 0.3 typical
    constexpr auto penetration_slop = 0.003f;   // match positional correction slop
    constexpr auto max_bias_velocity = 2.0f;    // meters/second

    constexpr auto friction_mu = 0.6f; // friction (placeholder constant; plug in material mixing later)

    constexpr auto warm_start_scale = 1.0f;

    // build solver constraints
    auto solver_contacts = std::vector<solver_contact>{};
    solver_contacts.reserve(collisions.size() * 4u);

    for (const auto& collision_entry : collisions) {
      const auto node_a = collision_entry.node_a;
      const auto node_b = collision_entry.node_b;

      auto& rigidbody_a = scene.get_component<physics::rigidbody>(node_a);
      auto& rigidbody_b = scene.get_component<physics::rigidbody>(node_b);

      if (rigidbody_a.is_static() && rigidbody_b.is_static()) {
        continue;
      }

      const auto world_position_a = scene.world_position(node_a);
      const auto world_position_b = scene.world_position(node_b);

      const auto rotation_matrix_a = math::matrix_cast<math::matrix3x3>(scene.world_rotation(node_a));
      const auto rotation_matrix_b = math::matrix_cast<math::matrix3x3>(scene.world_rotation(node_b));

      const auto inverse_rotation_matrix_a = math::matrix3x3::transposed(rotation_matrix_a);
      const auto inverse_rotation_matrix_b = math::matrix3x3::transposed(rotation_matrix_b);

      // Ensure manifold normal points A -> B
      auto contact_normal = collision_entry.manifold.normal;
      const auto delta_ab = world_position_b - world_position_a;

      if (math::vector3::dot(delta_ab, contact_normal) < 0.0f) {
        contact_normal = -contact_normal;
      }

      const auto contact_count = collision_entry.manifold.contact_points.size();

      if (contact_count == 0u) {
        continue;
      }

      // Shared values for all contacts in this manifold
      const auto inverse_mass_a = rigidbody_a.inverse_mass();
      const auto inverse_mass_b = rigidbody_b.inverse_mass();

      const auto inverse_inertia_tensor_world_a = rigidbody_a.inverse_inertia_tensor_world();
      const auto inverse_inertia_tensor_world_b = rigidbody_b.inverse_inertia_tensor_world();

      // bias velocity (same per manifold)
      const auto penetration_depth = collision_entry.manifold.depth;
      const auto penetration_error = std::max(penetration_depth - penetration_slop, 0.0f);

      auto bias_velocity = 0.0f;

      if (dt > 0.0f) {
        bias_velocity = std::clamp(baumgarte_beta * penetration_error / dt, 0.0f, max_bias_velocity);
      }

      // create one solver contact per contact point
      for (auto contact_point_index = std::size_t{0u}; contact_point_index < contact_count; ++contact_point_index) {
        const auto contact_point_world = collision_entry.manifold.contact_points[contact_point_index];

        const auto r_a_world = contact_point_world - world_position_a;
        const auto r_b_world = contact_point_world - world_position_b;

        const auto local_anchor_a = inverse_rotation_matrix_a * r_a_world;
        const auto local_anchor_b = inverse_rotation_matrix_b * r_b_world;

        auto constraint = contact_constraint{};
        constraint.normal = contact_normal;

        constraint.r_a = contact_point_world - world_position_a;
        constraint.r_b = contact_point_world - world_position_b;

        // Relative velocity at the contact
        const auto velocity_at_contact_a = rigidbody_a.velocity() + math::vector3::cross(rigidbody_a.angular_velocity(), constraint.r_a);

        const auto velocity_at_contact_b = rigidbody_b.velocity() + math::vector3::cross(rigidbody_b.angular_velocity(), constraint.r_b);

        const auto relative_velocity = velocity_at_contact_b - velocity_at_contact_a;

        // Tangent (opposes tangential motion)
        auto contact_tangent = relative_velocity - contact_normal * math::vector3::dot(relative_velocity, contact_normal);

        if (contact_tangent.length_squared() > 1e-12f) {
          contact_tangent = contact_tangent / std::sqrt(contact_tangent.length_squared());
        } else {
          contact_tangent = _any_perpendicular(contact_normal);
        }

        constraint.tangent = contact_tangent;

        // effective mass (normal)
        {
          const auto r_a_cross_n = math::vector3::cross(constraint.r_a, contact_normal);
          const auto r_b_cross_n = math::vector3::cross(constraint.r_b, contact_normal);

          const auto angular_term_a = math::vector3::dot(contact_normal, math::vector3::cross(inverse_inertia_tensor_world_a * r_a_cross_n, constraint.r_a));
          const auto angular_term_b = math::vector3::dot(contact_normal, math::vector3::cross(inverse_inertia_tensor_world_b * r_b_cross_n, constraint.r_b));

          const auto effective_mass = inverse_mass_a + inverse_mass_b + angular_term_a + angular_term_b;

          constraint.normal_mass = (effective_mass > 1e-12f) ? (1.0f / effective_mass) : 0.0f;
        }

        // effective mass (tangent)
        {
          const auto r_a_cross_t = math::vector3::cross(constraint.r_a, contact_tangent);
          const auto r_b_cross_t = math::vector3::cross(constraint.r_b, contact_tangent);

          const auto angular_term_a = math::vector3::dot(contact_tangent, math::vector3::cross(inverse_inertia_tensor_world_a * r_a_cross_t, constraint.r_a));
          const auto angular_term_b = math::vector3::dot(contact_tangent, math::vector3::cross(inverse_inertia_tensor_world_b * r_b_cross_t, constraint.r_b));

          const auto effective_mass = inverse_mass_a + inverse_mass_b + angular_term_a + angular_term_b;

          constraint.tangent_mass = (effective_mass > 1e-12f) ? (1.0f / effective_mass) : 0.0f;
        }

        // bias
        constraint.bias = bias_velocity;

        // warm start (cached impulses)
        const auto contact_key = _contact_key(node_a, node_b, local_anchor_a, local_anchor_b);

        if (auto cached_it = _contact_cache.find(contact_key); cached_it != _contact_cache.end()) {
          constraint.normal_impulse  = cached_it->second.normal_impulse  * warm_start_scale;
          constraint.tangent_impulse = cached_it->second.tangent_impulse * warm_start_scale;
        }

        constraint.last_seen = _solver_tick;

        solver_contacts.push_back({node_a, node_b, contact_key, constraint});
      }
    }

    // warm start (apply cached impulses once before iterations)
    for (auto& solver_contact_entry : solver_contacts) {
      auto& rigidbody_a = scene.get_component<physics::rigidbody>(solver_contact_entry.node_a);
      auto& rigidbody_b = scene.get_component<physics::rigidbody>(solver_contact_entry.node_b);

      const auto warm_start_impulse = solver_contact_entry.constraint.normal  * solver_contact_entry.constraint.normal_impulse + solver_contact_entry.constraint.tangent * solver_contact_entry.constraint.tangent_impulse;

      _apply_impulse(rigidbody_a, rigidbody_b, warm_start_impulse, solver_contact_entry.constraint.r_a, solver_contact_entry.constraint.r_b);
    }

    // sequential impulse solver
    for (auto iteration_index = 0; iteration_index < solver_iterations; ++iteration_index) {
      for (auto& solver_contact_entry : solver_contacts) {
        auto& rigidbody_a = scene.get_component<physics::rigidbody>(solver_contact_entry.node_a);
        auto& rigidbody_b = scene.get_component<physics::rigidbody>(solver_contact_entry.node_b);

        // Recompute relative velocity at the contact
        const auto velocity_at_contact_a = rigidbody_a.velocity() + math::vector3::cross(rigidbody_a.angular_velocity(), solver_contact_entry.constraint.r_a);
        const auto velocity_at_contact_b = rigidbody_b.velocity() + math::vector3::cross(rigidbody_b.angular_velocity(), solver_contact_entry.constraint.r_b);

        const auto relative_velocity = velocity_at_contact_b - velocity_at_contact_a;

        // normal impulse
        const auto normal_relative_speed = math::vector3::dot(relative_velocity, solver_contact_entry.constraint.normal);

        auto normal_lambda = solver_contact_entry.constraint.normal_mass * (-(normal_relative_speed) + solver_contact_entry.constraint.bias);

        const auto previous_normal_impulse = solver_contact_entry.constraint.normal_impulse;

        solver_contact_entry.constraint.normal_impulse = std::max(previous_normal_impulse + normal_lambda, 0.0f);

        normal_lambda = solver_contact_entry.constraint.normal_impulse - previous_normal_impulse;

        if (normal_lambda != 0.0f) {
          const auto normal_impulse = solver_contact_entry.constraint.normal * normal_lambda;

          _apply_impulse(rigidbody_a, rigidbody_b, normal_impulse, solver_contact_entry.constraint.r_a, solver_contact_entry.constraint.r_b);
        }

        // friction impulse
        const auto tangent_relative_speed = math::vector3::dot(relative_velocity, solver_contact_entry.constraint.tangent);

        auto tangent_lambda = solver_contact_entry.constraint.tangent_mass * (-(tangent_relative_speed));

        const auto max_friction_impulse = friction_mu * solver_contact_entry.constraint.normal_impulse;

        const auto previous_tangent_impulse = solver_contact_entry.constraint.tangent_impulse;

        solver_contact_entry.constraint.tangent_impulse = std::clamp(previous_tangent_impulse + tangent_lambda, -max_friction_impulse, max_friction_impulse);

        tangent_lambda = solver_contact_entry.constraint.tangent_impulse - previous_tangent_impulse;

        if (tangent_lambda != 0.0f) {
          const auto tangent_impulse = solver_contact_entry.constraint.tangent * tangent_lambda;

          _apply_impulse(rigidbody_a, rigidbody_b, tangent_impulse, solver_contact_entry.constraint.r_a, solver_contact_entry.constraint.r_b);
        }
      }
    }

    // write back cache + prune stale constraints
    _update_cache(solver_contacts);
  }

  auto _update_character_controllers() -> void {
    SBX_PROFILE_SCOPE("physics_module::_update_character_controllers");

    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();


  }

  auto _update_cache(const std::vector<solver_contact>& solver_contacts) -> void {
    // prune old cached constraints after some frames
    constexpr auto cache_ttl = std::uint64_t{60};

    for (auto& solver_contact_entry : solver_contacts) {
      _contact_cache[solver_contact_entry.key] = solver_contact_entry.constraint;
    }

    for (auto entry = _contact_cache.begin(); entry != _contact_cache.end(); ) {
      const auto is_expired = (_solver_tick - entry->second.last_seen) > cache_ttl;

      entry = is_expired ? _contact_cache.erase(entry) : ++entry;
    }
  }

  auto _positional_correction(const std::vector<collision>& collisions) -> void {
    auto& scene = core::engine::get_module<scenes::scenes_module>().scene();

    constexpr auto correction_percent = 0.8f;
    constexpr auto penetration_slop = 0.003f;
    constexpr auto max_correction = 0.05f;

    for (const auto& collision_pair : collisions) {
      auto& rigidbody_a = scene.get_component<physics::rigidbody>(collision_pair.node_a);
      auto& rigidbody_b = scene.get_component<physics::rigidbody>(collision_pair.node_b);

      if (rigidbody_a.is_static() && rigidbody_b.is_static()) {
        continue;
      }

      auto& transform_a = scene.get_component<scenes::transform>(collision_pair.node_a);
      auto& transform_b = scene.get_component<scenes::transform>(collision_pair.node_b);

      auto contact_normal = collision_pair.manifold.normal;

      const auto world_position_a = scene.world_position(collision_pair.node_a);
      const auto world_position_b = scene.world_position(collision_pair.node_b);
      const auto delta_ab = world_position_b - world_position_a;

      if (math::vector3::dot(delta_ab, contact_normal) < 0.0f) {
        contact_normal = -contact_normal;
      }

      const auto inverse_mass_a = rigidbody_a.inverse_mass();
      const auto inverse_mass_b = rigidbody_b.inverse_mass();
      const auto inverse_mass_sum = inverse_mass_a + inverse_mass_b;

      if (inverse_mass_sum <= 0.0f) {
        continue;
      }

      const auto penetration_depth = collision_pair.manifold.depth;
      const auto penetration_error = std::max(penetration_depth - penetration_slop, 0.0f);

      const auto raw_correction = correction_percent * penetration_error / inverse_mass_sum;
      const auto clamped_correction = std::min(raw_correction, max_correction);

      transform_a.position() -= contact_normal * (clamped_correction * inverse_mass_a);
      transform_b.position() += contact_normal * (clamped_correction * inverse_mass_b);

      transform_a.bump_version();
      transform_b.bump_version();
    }
  }

  static auto _quantize_scalar(std::float_t value, std::float_t grid) -> std::int32_t {
    return static_cast<std::int32_t>(std::lround(value / grid));
  }

  static auto _quantize_vector3(const math::vector3& value, std::float_t grid) -> std::array<std::int32_t, 3u> {
    return {_quantize_scalar(value.x(), grid), _quantize_scalar(value.y(), grid), _quantize_scalar(value.z(), grid)};
  }

  static auto _contact_key(scenes::node node_a, scenes::node node_b, const math::vector3& local_anchor_a, const math::vector3& local_anchor_b) -> std::size_t {
    // 1 cm bins (tune 0.005f - 0.02f)
    constexpr auto anchor_grid = std::float_t{0.01f};

    const auto quant_a = _quantize_vector3(local_anchor_a, anchor_grid);
    const auto quant_b = _quantize_vector3(local_anchor_b, anchor_grid);

    auto hash = std::size_t{0};

    utility::hash_combine(hash, node_a, node_b, quant_a[0], quant_a[1], quant_a[2], quant_b[0], quant_b[1], quant_b[2]);

    return hash;
  }

  static auto _any_perpendicular(const math::vector3& n) -> math::vector3 {
    const auto ax = std::abs(n.x()) < 0.9f ? math::vector3{1.0f, 0.0f, 0.0f} : math::vector3{0.0f, 1.0f, 0.0f};
  
    auto tangent = math::vector3::cross(ax, n);

    return math::vector3::normalized(tangent);
  }

  static auto _apply_impulse(physics::rigidbody& a, physics::rigidbody& b, const math::vector3& impulse, const math::vector3& r_a, const math::vector3& r_b) -> void {
    const auto inverse_mass_a = a.inverse_mass();
    const auto inverse_mass_b = b.inverse_mass();

    if (inverse_mass_a > 0.0f) {
      a.add_velocity(-impulse * inverse_mass_a);
      a.add_angular_velocity(-(a.inverse_inertia_tensor_world() * math::vector3::cross(r_a, impulse)));
    }

    if (inverse_mass_b > 0.0f) {
      b.add_velocity(impulse * inverse_mass_b);
      b.add_angular_velocity( (b.inverse_inertia_tensor_world() * math::vector3::cross(r_b, impulse)));
    }
  }

  std::unordered_map<std::size_t, contact_constraint> _contact_cache{};
  std::uint64_t _solver_tick{0};

}; // class physics_module

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
