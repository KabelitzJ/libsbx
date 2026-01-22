// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PHYSICS_COLLIDER_HPP_
#define LIBSBX_PHYSICS_COLLIDER_HPP_

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
}; // struct sphere

struct cylinder {
  std::float_t radius{0.5f};
  std::float_t half_height{0.5f};
}; // struct cylinder

struct capsule {
  std::float_t radius{0.5f};
  std::float_t half_height{0.5f};
}; // struct capsule

struct box {
  math::vector3 half_extents{0.5f, 0.5f, 0.5f};
}; // struct box

using collider_shape = std::variant<sphere, cylinder, capsule, box>;

struct collider {
  collider_shape shape;
  math::vector3 offset{math::vector3::zero};
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};
}; // struct collider

auto inverse_inertia_tensor(const collider& collider, std::float_t mass) -> math::matrix3x3;

auto get_bounding_volume(const collider& collider, const math::matrix4x4& transform) -> math::volume;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_COLLIDER_HPP_

