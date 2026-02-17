// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PHYSICS_CHARACTER_CONTROLLER_HPP_
#define LIBSBX_PHYSICS_CHARACTER_CONTROLLER_HPP_

#include <libsbx/utility/enum.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>

#include <libsbx/physics/shape_collider.hpp>
#include <libsbx/physics/mesh_collider.hpp>
#include <libsbx/physics/collision_detection.hpp>

namespace sbx::physics {

enum class collision_flags : std::uint8_t {
  none  = 0,
  below = utility::bit_v<0>,
  above = utility::bit_v<1>,
  sides = utility::bit_v<2>
}; // enum class collision_flags

constexpr auto operator|(collision_flags lhs, collision_flags rhs) -> collision_flags {
  return static_cast<collision_flags>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

constexpr auto operator&(collision_flags lhs, collision_flags rhs) -> collision_flags {
  return static_cast<collision_flags>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
}

constexpr auto operator|=(collision_flags& lhs, collision_flags rhs) -> collision_flags& {
  lhs = lhs | rhs;

  return lhs;
}

struct move_result {
  math::vector3 position;
  collision_flags flags{collision_flags::none};
  math::vector3 ground_normal{0.0f, 1.0f, 0.0f};
  bool grounded{false};
}; // struct move_result

struct character_controller {
  std::float_t height{2.0f};
  std::float_t radius{0.5f};
  std::float_t slope_limit{45.0f};
  std::float_t step_offset{0.3f};
  std::float_t skin_width{0.01f};
  std::float_t ground_snap_distance{0.2f};
  bool was_grounded{false};
}; // struct character_controller

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_CHARACTER_CONTROLLER_HPP_