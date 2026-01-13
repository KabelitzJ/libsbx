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

public:

  physics_module() = default;

  ~physics_module() override = default;

  auto update() -> void override {
    SBX_PROFILE_SCOPE("physics_module::update");
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

      const auto rotation_matrix = math::matrix_cast<3, 3>(scene.world_rotation(node));
      rigidbody.update_inertia_tensor_world(rotation_matrix);

      const auto angular_acc = rigidbody.inverse_inertia_tensor_world() * rigidbody.torque();
      rigidbody.add_angular_velocity(angular_acc * dt);

      const auto max_angular_velocity = 15.0f;
      const auto angular_vel = rigidbody.angular_velocity();
      if (angular_vel.length_squared() > max_angular_velocity * max_angular_velocity) {
        rigidbody.set_angular_velocity(math::vector3::normalized(angular_vel) * max_angular_velocity);
      }

      const auto linear_damping = 0.98f;
      const auto angular_damping = 0.95f; 

      rigidbody.set_velocity(rigidbody.velocity() * linear_damping);
      rigidbody.set_angular_velocity(rigidbody.angular_velocity() * angular_damping);

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

      const auto current_position = scene.world_position(node);
      const auto current_rotation = scene.world_rotation(node);

      const auto next_position = current_position + rigidbody.velocity() * dt;

      const auto w_quat = math::quaternion{rigidbody.angular_velocity(), 0.0f};
      const auto next_rotation = math::quaternion::normalized(current_rotation + (w_quat * current_rotation) * (0.5f * dt));

      const auto parent = scene.get_component<scenes::relationship>(node).parent();

      if (parent == scenes::node::null) {
        transform.set_position(next_position);
        transform.set_rotation(next_rotation);
      } else {
        const auto inverse_parent_matrix = math::matrix4x4::inverted(scene.world_transform(parent));
        const auto inverse_parent_rotation = math::quaternion::inverted(scene.world_rotation(parent));

        transform.set_position(math::vector3{inverse_parent_matrix * math::vector4{next_position, 1.0f}});
        transform.set_rotation(inverse_parent_rotation * next_rotation);
      }

      transform.bump_version();
    }
  }

  auto _collision_broad_phase() -> std::vector<collision_pair_type> {
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto tree = octree_type{math::volume{math::vector3{-500.0f}, math::vector3{500.0f}}, 64u};

    auto query = scene.query<const physics::collider, const physics::rigidbody>();

    for (auto&& [node, collider, rigidbody] : query.each()) {
      const auto world_transform = scene.world_transform(node);
      const auto volume = get_bounding_volume(collider, world_transform);

      tree.insert(node, volume);
    }

    return tree.intersections();
  }

  // auto _collision_narrow_phase(const std::vector<collision_pair_type>& pairs) -> std::vector<collision> {
  //   auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  //   auto& scene = scenes_module.scene();

  //   auto collisions = std::vector<collision>{};
  //   collisions.reserve(pairs.size());

  //   for (const auto& pair : pairs) {
  //     const auto [node_a, node_b] = pair;

  //     auto& rigidbody_a = scene.get_component<physics::rigidbody>(node_a);
  //     auto& rigidbody_b = scene.get_component<physics::rigidbody>(node_b);

  //     if (rigidbody_a.is_static() && rigidbody_b.is_static()) {
  //       continue;
  //     }

  //     const auto& collider_a = scene.get_component<physics::collider>(node_a);
  //     const auto& collider_b = scene.get_component<physics::collider>(node_b);
  //   }

  //   return collisions;
  // }

  // auto _resolve_collisions(const std::vector<collision>& collisions, std::float_t dt) -> void {
    
  // }

}; // class physics_module

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_PHYSICS_MODULE_HPP_
