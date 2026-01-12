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
  std::float_t radius{0.5f};
}; 

struct cylinder {
  std::float_t radius{0.5f};
  std::float_t base{-0.5f};
  std::float_t cap{0.5f};
};

struct capsule {
  std::float_t radius{0.5f};
  std::float_t base{-0.5f};
  std::float_t cap{0.5f};
};

struct box {
  math::vector3 half_extents{0.5f, 0.5f, 0.5f};
};

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
  const auto m = std::max(mass.value(), 0.0001f);
  const auto size = box.half_extents * 2.0f;

  const auto x2 = size.x() * size.x();
  const auto y2 = size.y() * size.y();
  const auto z2 = size.z() * size.z();

  const auto inv_mass_12 = 12.0f / m;

  return math::matrix3x3{
    inv_mass_12 / (y2 + z2), 0.0f, 0.0f,
    0.0f, inv_mass_12 / (x2 + z2), 0.0f,
    0.0f, 0.0f, inv_mass_12 / (x2 + y2)
  };
}

inline auto local_inverse_inertia(const units::kilogram& mass, const cylinder& cylinder) -> math::matrix3x3 {
  const auto m = std::max(mass.value(), 0.0001f);
  const auto r2 = cylinder.radius * cylinder.radius;
  const auto h2 = (cylinder.base + cylinder.cap) * (cylinder.base + cylinder.cap); // Assuming total height
  
  // Cylinder along Y-axis usually
  const auto i_x_z = (1.0f / 12.0f) * m * (3 * r2 + h2);
  const auto i_y = 0.5f * m * r2;

  return math::matrix3x3{
    1.0f / i_x_z, 0.0f, 0.0f,
    0.0f, 1.0f / i_y, 0.0f,
    0.0f, 0.0f, 1.0f / i_x_z
  };
}

inline auto local_inverse_inertia(const units::kilogram& mass, const sphere& sphere) -> math::matrix3x3 {
  const auto m = std::max(mass.value(), 0.0001f);
  // Solid sphere inertia: (2/5) * m * r^2
  const auto i = (2.0f / 5.0f) * m * sphere.radius * sphere.radius;
  const auto inv_i = 1.0f / i;

  return math::matrix3x3{
    inv_i, 0.0f, 0.0f,
    0.0f, inv_i, 0.0f,
    0.0f, 0.0f, inv_i
  };
}

inline auto local_inverse_inertia(const units::kilogram& mass, const capsule& capsule) -> math::matrix3x3 {
  auto approx_cyl = cylinder{};
  approx_cyl.radius = capsule.radius;
  approx_cyl.base = capsule.base; 
  approx_cyl.cap = capsule.cap;

  return local_inverse_inertia(mass, approx_cyl);
}

inline auto local_inverse_inertia(const units::kilogram& mass, const collider& collider) -> math::matrix3x3 {
  return std::visit([&](const auto& shape) { return local_inverse_inertia(mass, shape); }, collider);
}

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_BOX_COLLIDER_HPP_

