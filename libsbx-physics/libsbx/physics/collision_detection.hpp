// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PHYSICS_COLLISION_DETECTION_HPP_
#define LIBSBX_PHYSICS_COLLISION_DETECTION_HPP_

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/volume.hpp>
#include <libsbx/math/quaternion.hpp>

#include <libsbx/scenes/node.hpp>

#include <libsbx/physics/shape_collider.hpp>
#include <libsbx/physics/mesh_collider.hpp>

namespace sbx::physics {

struct collider_data {
  math::vector3 position;
  math::quaternion rotation;
  const physics::shape_collider& collider;
}; // struct collider_data

struct collision_manifold {
  math::vector3 normal;
  float depth{0.0f};
  std::vector<math::vector3> contact_points;
}; // struct collision_manifold

auto check_collision(const collider_data& first, const collider_data& second) -> std::optional<collision_manifold>;

auto check_collision(const collider_data& convex, const mesh_collider& mesh, const math::vector3& mesh_position, const math::quaternion& mesh_rotation) -> std::optional<collision_manifold>;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_COLLISION_DETECTION_HPP_