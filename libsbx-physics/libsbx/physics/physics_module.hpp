#ifndef LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
#define LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_

#include <cmath>
#include <optional>
#include <vector>
#include <algorithm>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/module.hpp>

#include <libsbx/containers/octree.hpp>

#include <libsbx/math/constants.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/matrix_cast.hpp>

#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/rigidbody.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scenes/components/global_transform.hpp>
#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/relationship.hpp>

#include <libsbx/utility/logger.hpp>

namespace sbx::physics {

class physics_module : public core::module<physics_module> {

  inline static const auto is_registered = register_module(stage::fixed, dependencies<scenes::scenes_module>{});

  using octree_type = containers::octree<scenes::node, 16u, 8u>;

  using collision_pair_type = typename octree_type::intersection;

  struct collision {
    collision_pair_type nodes;
    collision_manifold manifold;
  }; // struct collision

public:

  physics_module() = default;

  ~physics_module() override = default;

  auto update() -> void override {
    SBX_PROFILE_SCOPE("physics_module::update");

    const auto dt = core::engine::fixed_delta_time().value();

    _integrate_velocities(dt);

    const auto intersections = _collision_broad_phase();
    const auto collisions = _collision_narrow_phase(intersections);

    _resolve_collisions(collisions, dt);

    _integrate_positions(dt);
  }

private:

  auto _integrate_velocities(std::float_t dt) -> void {
    auto& scene = core::engine::get_module<scenes::scenes_module>().scene();
    auto query = scene.query<physics::rigidbody>();

    for (auto&& [node, rigidbody] : query.each()) {
      if (rigidbody.is_static()) {
        continue;
      }

      const auto total_forces = rigidbody.constant_forces() + rigidbody.dynamic_forces();

      const auto acceleration = total_forces * rigidbody.inverse_mass();
      rigidbody.add_velocity(acceleration * dt);

      rigidbody.update_inertia_tensor_world(math::matrix_cast<3, 3>(scene.world_rotation(node)));

      const auto angular_acc = rigidbody.inverse_inertia_tensor_world() * rigidbody.torque();
      rigidbody.add_angular_velocity(angular_acc * dt);

      // --- STABILIZATION: Damping ---
      // Bleed out energy to prevent micro-jitters from accumulating
      const float linear_damping = 0.98f;
      const float angular_damping = 0.95f; 
      rigidbody.set_velocity(rigidbody.velocity() * linear_damping);
      rigidbody.set_angular_velocity(rigidbody.angular_velocity() * angular_damping);

      rigidbody.clear_dynamic_forces();
      rigidbody.clear_torque();
    }
  }

  auto _integrate_positions(std::float_t dt) -> void {
    auto& scene = core::engine::get_module<scenes::scenes_module>().scene();
    auto query = scene.query<scenes::transform, physics::rigidbody>();

    for (auto&& [node, transform, rigidbody] : query.each()) {
      if (rigidbody.is_static()) {
        continue;
      }

      const auto current_pos = scene.world_position(node);
      const auto current_rot = scene.world_rotation(node);

      const auto next_pos = current_pos + rigidbody.velocity() * dt;

      const auto w_quat = math::quaternion{rigidbody.angular_velocity(), 0.0f};
      const auto next_rot = math::quaternion::normalized(current_rot + (w_quat * current_rot) * (0.5f * dt));

      const auto parent = scene.get_component<scenes::relationship>(node).parent();

      if (parent == scenes::node::null) {
        transform.set_position(next_pos);
        transform.set_rotation(next_rot);
      } else {
        const auto inv_parent_matrix = math::matrix4x4::inverted(scene.world_transform(parent));
        const auto inv_parent_rot = math::quaternion::inverted(scene.world_rotation(parent));

        transform.set_position(math::vector3{inv_parent_matrix * math::vector4{next_pos, 1.0f}});
        transform.set_rotation(inv_parent_rot * next_rot);
      }

      transform.bump_version();
    }
  }

  auto _collision_broad_phase() -> std::vector<collision_pair_type> {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto tree = octree_type{math::volume{math::vector3{-1000.0f}, math::vector3{1000.0f}}, 100u};

    auto query = scene.query<const physics::collider, const physics::rigidbody>();

    for (auto&& [node, collider, rigidbody] : query.each()) {
      const auto volume = bounding_volume(collider, scene.world_position(node));
      tree.insert(node, volume);
    }

    return tree.intersections();
  }

  auto _collision_narrow_phase(const std::vector<collision_pair_type>& pairs) -> std::vector<collision> {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto collisions = utility::make_reserved_vector<collision>(pairs.size());

    for (const auto& pair : pairs) {
      const auto [node_a, node_b] = pair;

      auto& rigidbody_a = scene.get_component<physics::rigidbody>(node_a);
      auto& rigidbody_b = scene.get_component<physics::rigidbody>(node_b);

      if (rigidbody_a.is_static() && rigidbody_b.is_static()) {
        continue;
      }

      const auto& collider_a = scene.get_component<physics::collider>(node_a);
      auto rot_scale_a = scene.world_transform(node_a);
      rot_scale_a[3] = math::vector4{0.0f, 0.0f, 0.0f, 1.0f};
      const auto data_a = collider_data{scene.world_position(node_a), rot_scale_a, collider_a};

      const auto& collider_b = scene.get_component<physics::collider>(node_b);
      auto rot_scale_b = scene.world_transform(node_b);
      rot_scale_b[3] = math::vector4{0.0f, 0.0f, 0.0f, 1.0f};
      const auto data_b = collider_data{scene.world_position(node_b), rot_scale_b, collider_b};

      if (auto manifold = gjk(data_a, data_b); manifold) {
        collisions.push_back(collision{pair, *manifold});
      }
    }

    return collisions;
  }

  auto _resolve_collisions(const std::vector<collision>& collisions, std::float_t dt) -> void {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    struct solver_constraint {
      scenes::node node_a;
      scenes::node node_b;
      math::vector3 r_a;
      math::vector3 r_b;
      math::vector3 normal;
      float bias;
      float effective_mass;
      float accumulated_impulse = 0.0f;
    };

    std::vector<solver_constraint> constraints;
    constraints.reserve(collisions.size() * 4);

    const auto max_penetration_vel = 2.0f; 
    const auto slop = 0.01f;
    const auto baumgarte = 0.15f; // Slightly lower for stability

    for (const auto& [nodes, manifold] : collisions) {
      auto& rb_a = scene.get_component<physics::rigidbody>(nodes.first);
      auto& rb_b = scene.get_component<physics::rigidbody>(nodes.second);

      if (rb_a.inverse_mass() + rb_b.inverse_mass() <= 0.0f) continue;

      const auto ab = scene.world_position(nodes.second) - scene.world_position(nodes.first);
      const auto normal = (math::vector3::dot(ab, manifold.normal) < 0.0f) ? -manifold.normal : manifold.normal;

      for (const auto& point : manifold.contact_points) {
        const auto r_a = point - scene.world_position(nodes.first);
        const auto r_b = point - scene.world_position(nodes.second);
        const auto rn_a = math::vector3::cross(r_a, normal);
        const auto rn_b = math::vector3::cross(r_b, normal);
        
        const auto inv_mass_a = rb_a.inverse_mass();
        const auto inv_mass_b = rb_b.inverse_mass();
        const auto inertia_a = (inv_mass_a > 0.0f) ? math::vector3::dot(rb_a.inverse_inertia_tensor_world() * rn_a, rn_a) : 0.0f;
        const auto inertia_b = (inv_mass_b > 0.0f) ? math::vector3::dot(rb_b.inverse_inertia_tensor_world() * rn_b, rn_b) : 0.0f;
        const auto effective_mass = inv_mass_a + inv_mass_b + inertia_a + inertia_b;

        if (effective_mass > 0.0f) {
          auto& c = constraints.emplace_back();
          c.node_a = nodes.first;
          c.node_b = nodes.second;
          c.r_a = r_a;
          c.r_b = r_b;
          c.normal = normal;
          c.effective_mass = effective_mass;
          
          float penetration_depth = std::max(0.0f, manifold.depth - slop);
          c.bias = (baumgarte / dt) * penetration_depth;
          c.bias = std::min(c.bias, max_penetration_vel);
        }
      }
    }

    const int solver_iterations = 10; 

    for (int i = 0; i < solver_iterations; ++i) {
      for (auto& c : constraints) {
        auto& rb_a = scene.get_component<physics::rigidbody>(c.node_a);
        auto& rb_b = scene.get_component<physics::rigidbody>(c.node_b);

        const auto v_total_a = rb_a.velocity() + math::vector3::cross(rb_a.angular_velocity(), c.r_a);
        const auto v_total_b = rb_b.velocity() + math::vector3::cross(rb_b.angular_velocity(), c.r_b);
        const auto rel_vel = v_total_b - v_total_a;
        
        // --- NORMAL IMPULSE ---
        const float vn = math::vector3::dot(rel_vel, c.normal);
        float lambda_n = (-vn + c.bias) / c.effective_mass;

        float old_impulse_n = c.accumulated_impulse;
        c.accumulated_impulse = std::max(0.0f, old_impulse_n + lambda_n);
        float delta_n = c.accumulated_impulse - old_impulse_n;

        rb_a.apply_impulse_at(-c.normal * delta_n, c.r_a);
        rb_b.apply_impulse_at( c.normal * delta_n, c.r_b);

        // --- FRICTION IMPULSE (Tangent) ---
        // Recalculate velocity after normal impulse
        const auto v_post_a = rb_a.velocity() + math::vector3::cross(rb_a.angular_velocity(), c.r_a);
        const auto v_post_b = rb_b.velocity() + math::vector3::cross(rb_b.angular_velocity(), c.r_b);
        const auto rel_vel_f = v_post_b - v_post_a;

        math::vector3 tangent = rel_vel_f - (c.normal * math::vector3::dot(rel_vel_f, c.normal));
        
        if (tangent.length_squared() > 1e-6f) {
          tangent = math::vector3::normalized(tangent);
          float vt = math::vector3::dot(rel_vel_f, tangent);
          
          // Friction lambda (simple friction uses same effective mass or approximated)
          float lambda_t = -vt / c.effective_mass;

          // Coulomb Friction: Friction <= Normal Force * mu
          const float mu = 0.4f; 
          float max_f = c.accumulated_impulse * mu;
          lambda_t = std::clamp(lambda_t, -max_f, max_f);

          rb_a.apply_impulse_at(-tangent * lambda_t, c.r_a);
          rb_b.apply_impulse_at( tangent * lambda_t, c.r_b);
        }
      }
    }
  }

}; // class physics_module

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_