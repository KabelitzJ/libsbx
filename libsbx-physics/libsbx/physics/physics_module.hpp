#ifndef LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
#define LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_

#include <cmath>
#include <optional>
#include <vector>
#include <algorithm>
#include <ranges>

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
  }; 

  struct solver_constraint {
    scenes::node node_a;
    scenes::node node_b;
    
    math::vector3 r_a; // Vector from Center of Mass A to Contact Point
    math::vector3 r_b; // Vector from Center of Mass B to Contact Point
    
    math::vector3 normal;
    math::vector3 tangent1;
    math::vector3 tangent2;

    float penetration;
    float normal_mass;    // Effective mass for normal impulse
    float tangent_mass;   // Effective mass for friction impulse
    
    float friction;
    float restitution;
    float bias;           // Baumgarte stabilization term
    
    float accumulated_normal_impulse = 0.0f;
    float accumulated_tangent_impulse_1 = 0.0f;
    float accumulated_tangent_impulse_2 = 0.0f;
  };

public:

  physics_module() = default;
  ~physics_module() override = default;

  auto update() -> void override {
    // SBX_PROFILE_SCOPE("physics_module::update");

    const auto dt = core::engine::fixed_delta_time().value();

    // 1. Apply Gravity and Forces
    _integrate_velocities(dt);

    // 2. Broad Phase (Find potential pairs)
    const auto intersections = _collision_broad_phase();

    // 3. Narrow Phase (Generate Manifolds)
    const auto collisions = _collision_narrow_phase(intersections);

    // 4. Solve Constraints (Resolve Overlaps and Velocity)
    _resolve_collisions(collisions, dt);

    // 5. Update Positions (Apply Velocity)
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

      // Update World Inertia Tensor based on current rotation
      const auto rotation_matrix = math::matrix_cast<3, 3>(scene.world_rotation(node));
      rigidbody.update_inertia_tensor_world(rotation_matrix);

      const auto angular_acc = rigidbody.inverse_inertia_tensor_world() * rigidbody.torque();
      rigidbody.add_angular_velocity(angular_acc * dt);

      // Cap angular velocity to prevent instability at high speeds
      const auto max_angular_velocity = 15.0f;
      const auto angular_vel = rigidbody.angular_velocity();
      if (angular_vel.length_squared() > max_angular_velocity * max_angular_velocity) {
        rigidbody.set_angular_velocity(math::vector3::normalized(angular_vel) * max_angular_velocity);
      }

      // Damping
      const auto linear_damping = 0.98f;
      const auto angular_damping = 0.95f; 

      rigidbody.set_velocity(rigidbody.velocity() * linear_damping);
      rigidbody.set_angular_velocity(rigidbody.angular_velocity() * angular_damping);

      // Sleeping threshold
      const auto sleep_threshold = 0.005f; 
      if (rigidbody.velocity().length_squared() < sleep_threshold && rigidbody.angular_velocity().length_squared() < sleep_threshold) {
        rigidbody.set_velocity(math::vector3::zero);
        rigidbody.set_angular_velocity(math::vector3::zero);
      }

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

      // Symplectic Euler: Use new velocity for position
      const auto next_pos = current_pos + rigidbody.velocity() * dt;

      // Angular integration: q_new = q + 0.5 * w * q * dt
      const auto w_quat = math::quaternion{rigidbody.angular_velocity(), 0.0f};
      const auto next_rot = math::quaternion::normalized(current_rot + (w_quat * current_rot) * (0.5f * dt));

      // Handle Parent/Child coordinate conversion
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

    // Rebuilding octree every frame is costly for complex scenes.
    // Optimization TODO: Use a persistent dynamic AABB tree or Broadphase SAP.
    auto tree = octree_type{math::volume{math::vector3{-500.0f}, math::vector3{500.0f}}, 64u};

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

    auto collisions = std::vector<collision>{};
    collisions.reserve(pairs.size());

    for (const auto& pair : pairs) {
      const auto [node_a, node_b] = pair;

      auto& rigidbody_a = scene.get_component<physics::rigidbody>(node_a);
      auto& rigidbody_b = scene.get_component<physics::rigidbody>(node_b);

      if (rigidbody_a.is_static() && rigidbody_b.is_static()) {
        continue;
      }

      const auto& collider_a = scene.get_component<physics::collider>(node_a);
      const auto& collider_b = scene.get_component<physics::collider>(node_b);

      // Prepare collider data (World Space)
      auto transform_a = scene.world_transform(node_a);
      transform_a[3] = math::vector4{0.0f, 0.0f, 0.0f, 1.0f}; // Remove translation for direction logic
      const auto data_a = collider_data{scene.world_position(node_a), transform_a, collider_a};

      auto transform_b = scene.world_transform(node_b);
      transform_b[3] = math::vector4{0.0f, 0.0f, 0.0f, 1.0f};
      const auto data_b = collider_data{scene.world_position(node_b), transform_b, collider_b};

      if (auto manifold = gjk(data_a, data_b); manifold) {
        collisions.push_back(collision{pair, *manifold});
      }
    }

    return collisions;
  }

  auto _resolve_collisions(const std::vector<collision>& collisions, std::float_t dt) -> void {
    if (collisions.empty()) return;

    auto& scene = core::engine::get_module<scenes::scenes_module>().scene();
    std::vector<solver_constraint> constraints;
    
    // 1. PRE-SOLVE: Generate Constraints
    for (const auto& col : collisions) {
      auto node_a = col.nodes.first;
      auto node_b = col.nodes.second;

      auto& body_a = scene.get_component<physics::rigidbody>(node_a);
      auto& body_b = scene.get_component<physics::rigidbody>(node_b);

      // Default friction/restitution mixing
      // You should ideally get this from the collider component or a physics material
      float friction = 0.5f; 
      float restitution = 0.2f; 

      for (const auto& contact_point : col.manifold.contact_points) {
         solver_constraint c;
         c.node_a = node_a;
         c.node_b = node_b;
         c.normal = col.manifold.normal;
         c.penetration = col.manifold.depth;
         c.friction = friction;
         c.restitution = restitution;

         // Calculate r (vector from CoM to contact point)
         const auto pos_a = scene.world_position(node_a);
         const auto pos_b = scene.world_position(node_b);
         c.r_a = contact_point - pos_a;
         c.r_b = contact_point - pos_b;

         // --- Calculate Effective Mass (K matrix) ---
         // K = 1/ma + 1/mb + (r x n)^T * I_inv * (r x n)
         const auto ra_xn = math::vector3::cross(c.r_a, c.normal);
         const auto rb_xn = math::vector3::cross(c.r_b, c.normal);
         
         const float ang_a = math::vector3::dot(ra_xn, body_a.inverse_inertia_tensor_world() * ra_xn);
         const float ang_b = math::vector3::dot(rb_xn, body_b.inverse_inertia_tensor_world() * rb_xn);
         const float inv_mass_sum = body_a.inverse_mass() + body_b.inverse_mass();

         c.normal_mass = (inv_mass_sum + ang_a + ang_b > 0.0f) ? 1.0f / (inv_mass_sum + ang_a + ang_b) : 0.0f;

         // --- Friction Tangents ---
         // Helper to build a basis from normal
         if (std::abs(c.normal.x()) >= 0.57735f) {
            c.tangent1 = math::vector3{c.normal.y(), -c.normal.x(), 0.0f};
         } else {
            c.tangent1 = math::vector3{0.0f, c.normal.z(), -c.normal.y()};
         }
         c.tangent1 = math::vector3::normalized(c.tangent1);
         c.tangent2 = math::vector3::cross(c.normal, c.tangent1);

         // We compute a simplified tangent mass (using just one average tangent for stability)
         const auto ra_xt = math::vector3::cross(c.r_a, c.tangent1);
         const auto rb_xt = math::vector3::cross(c.r_b, c.tangent1);
         const float ang_t_a = math::vector3::dot(ra_xt, body_a.inverse_inertia_tensor_world() * ra_xt);
         const float ang_t_b = math::vector3::dot(rb_xt, body_b.inverse_inertia_tensor_world() * rb_xt);
         c.tangent_mass = (inv_mass_sum + ang_t_a + ang_t_b > 0.0f) ? 1.0f / (inv_mass_sum + ang_t_a + ang_t_b) : 0.0f;

         // --- Baumgarte Stabilization (Bias) ---
         // Prevents sinking by adding a correction velocity based on depth
         const float slop = 0.01f;
         const float beta = 0.2f; // Correction factor
         c.bias = -beta / dt * std::max(0.0f, c.penetration - slop);

         // Add restitution (bounciness) to bias
         // relative velocity along normal
         const auto v_a = body_a.velocity() + math::vector3::cross(body_a.angular_velocity(), c.r_a);
         const auto v_b = body_b.velocity() + math::vector3::cross(body_b.angular_velocity(), c.r_b);
         const auto v_rel = math::vector3::dot(c.normal, v_b - v_a);
         
         if (v_rel < -1.0f) { // Only bounce if impact velocity is significant
             c.bias += -c.restitution * v_rel;
         }

         constraints.push_back(c);
      }
    }

    // 2. SOLVER ITERATIONS (Sequential Impulse)
    // 10 iterations is a good balance for game physics
    constexpr auto iterations = 10u;

    for (auto i = 0u; i < iterations; ++i) {
      for (auto& c : constraints) {
        auto& body_a = scene.get_component<physics::rigidbody>(c.node_a);
        auto& body_b = scene.get_component<physics::rigidbody>(c.node_b);

        // --- Solve Normal Constraint ---
        {
          const auto v_a = body_a.velocity() + math::vector3::cross(body_a.angular_velocity(), c.r_a);
          const auto v_b = body_b.velocity() + math::vector3::cross(body_b.angular_velocity(), c.r_b);
          const auto v_rel = math::vector3::dot(c.normal, v_b - v_a);

          // Lambda = -Jv / (JMJ^T)
          float lambda = -(v_rel + c.bias) * c.normal_mass;

          // Clamp accumulation (Impulse must be pushing, not pulling)
          const float old_acc = c.accumulated_normal_impulse;
          c.accumulated_normal_impulse = std::max(0.0f, old_acc + lambda);
          lambda = c.accumulated_normal_impulse - old_acc;

          // Apply Normal Impulse
          const auto impulse = c.normal * lambda;
          body_a.apply_impulse_at(-impulse, c.r_a);
          body_b.apply_impulse_at(impulse, c.r_b);
        }

        // --- Solve Friction Constraint ---
        {
          const auto v_a = body_a.velocity() + math::vector3::cross(body_a.angular_velocity(), c.r_a);
          const auto v_b = body_b.velocity() + math::vector3::cross(body_b.angular_velocity(), c.r_b);
          
          const auto dv = v_b - v_a;
          const float vt = math::vector3::dot(dv, c.tangent1); // Velocity along tangent

          float lambda_t = -vt * c.tangent_mass;

          // Coulomb Friction Clamping: |Ft| <= u * |Fn|
          const float max_friction = c.friction * c.accumulated_normal_impulse;
          const float old_acc_t = c.accumulated_tangent_impulse_1;
          c.accumulated_tangent_impulse_1 = std::clamp(old_acc_t + lambda_t, -max_friction, max_friction);
          lambda_t = c.accumulated_tangent_impulse_1 - old_acc_t;

          const auto impulse_t = c.tangent1 * lambda_t;
          body_a.apply_impulse_at(-impulse_t, c.r_a);
          body_b.apply_impulse_at(impulse_t, c.r_b);
        }
      }
    }
  }

}; // class physics_module

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_