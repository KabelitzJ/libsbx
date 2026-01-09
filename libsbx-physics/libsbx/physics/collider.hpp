#ifndef LIBSBX_PHYSICS_BOX_COLLIDER_HPP_
#define LIBSBX_PHYSICS_BOX_COLLIDER_HPP_

#include <variant>
#include <algorithm>

#include <libsbx/units/mass.hpp>
#include <libsbx/units/distance.hpp>
#include <libsbx/units/time.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/volume.hpp>

namespace sbx::physics {

struct sphere {
  std::float_t radius;
}; // struct sphere

struct cylinder {
  std::float_t radius;
  std::float_t base;
  std::float_t cap;
}; // struct cylinder

struct capsule {
  std::float_t radius;
  std::float_t base;
  std::float_t cap;
}; // struct capsule

struct box {
  math::vector3 half_extents;
}; // struct box

using collider = std::variant<sphere, cylinder, capsule, box>;

auto bounding_volume(const collider& collider, const math::vector3& position) -> math::volume;

struct collider_data {
  math::vector3 position;
  math::matrix4x4 rotation_scale;
  physics::collider collider;
}; // struct collider_data

auto find_furthest_point(const collider_data& collider, const math::vector3& direction) -> math::vector3;

struct minkowski_vertex {
  math::vector3 minkowski_point; // point_a - point_b
  math::vector3 point_a;         // Support point on shape A
}; // struct minkowski_vertex 

auto support(const collider_data& first, const collider_data& second, const math::vector3& direction) -> minkowski_vertex;

struct collision_manifold {
  math::vector3 normal;
  float depth{0.0f};
  std::vector<math::vector3> contact_points;
}; // struct collision_manifold

auto gjk(const collider_data& first, const collider_data& second) -> std::optional<collision_manifold>;

inline auto local_inverse_inertia(const units::kilogram& mass, const box& box) -> math::matrix3x3 {
  const float m = std::max(mass.value(), 0.0001f);

  const auto size = box.half_extents * 2.0f;

  const float w = size.x();
  const float h = size.y();
  const float d = size.z();

  return math::matrix3x3{
    12.0f / (m * (h * h + d * d)), 0.0f, 0.0f,
    0.0f, 12.0f / (m * (w * w + d * d)), 0.0f,
    0.0f, 0.0f, 12.0f / (m * (w * w + h * h))
  };
}

inline auto local_inverse_inertia(const units::kilogram& mass, const cylinder& cylinder) -> math::matrix3x3 {
  return math::matrix3x3::zero;
}

inline auto local_inverse_inertia(const units::kilogram& mass, const sphere& sphere) -> math::matrix3x3 {
  return math::matrix3x3::zero;
}

inline auto local_inverse_inertia(const units::kilogram& mass, const capsule& capsule) -> math::matrix3x3 {
  return math::matrix3x3::zero;
}

inline auto local_inverse_inertia(const units::kilogram& mass, const collider& collider) -> math::matrix3x3 {
  return std::visit([&](const auto& shape) { return local_inverse_inertia(mass, shape); }, collider);
}

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_BOX_COLLIDER_HPP_

