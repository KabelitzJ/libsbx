// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_CHARACTER_CONTROLLER_HPP_
#define LIBSBX_PHYSICS_CHARACTER_CONTROLLER_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <libsbx/math/vector3.hpp>

#include <libsbx/scenes/node.hpp>

#include <libsbx/physics/shapes.hpp>

namespace sbx::physics {

inline constexpr auto collision_below = std::uint8_t{1u << 0u};
inline constexpr auto collision_above = std::uint8_t{1u << 1u};
inline constexpr auto collision_sides = std::uint8_t{1u << 2u};

/**
 * @brief A kinematic, always-upright capsule moved only by physics_module::move_character: it slides along walls, climbs steps and slopes up to its limits and snaps down stairs.
 *
 * Dynamic bodies are pushed off it; it is never pushed back. Not combined with a rigidbody on the same node.
 */
struct character_controller {
  std::float_t height{2.0f}; // including both caps
  std::float_t radius{0.5f};
  math::vector3 center{math::vector3::zero}; // the capsule's center relative to the node, in world axes
  std::float_t skin_width{0.02f}; // kept between the capsule and what it touches
  std::float_t slope_limit{45.0f}; // degrees; steeper surfaces are walls
  std::float_t step_offset{0.3f}; // the tallest step it walks up
  std::float_t min_move_distance{0.001f};
  // Runtime-only, not persisted.
  bool is_grounded{false};
  std::uint8_t collision_flags{0u}; // collision_below | collision_above | collision_sides, from the last move
  math::vector3 velocity{math::vector3::zero}; // the last move's actual motion per second, over one fixed step
  math::vector3 ground_normal{math::vector3::up};
  std::float_t fall_speed{0.0f}; // simple_move's accumulated gravity
}; // struct character_controller

[[nodiscard]] inline auto controller_capsule(const character_controller& controller) -> capsule {
  const auto radius = std::max(controller.radius, 0.01f);
  return capsule{radius, std::max(controller.height * 0.5f - radius, 0.0f)};
}

/** @brief Whether a surface with this normal counts as ground under @p cos_slope_limit. */
[[nodiscard]] inline auto is_walkable(const math::vector3& normal, std::float_t cos_slope_limit) -> bool {
  return math::vector3::dot(normal, math::vector3::up) >= cos_slope_limit;
}

/**
 * @brief The part of @p remaining that continues after hitting a surface with @p normal: projected onto the surface, and for unwalkable surfaces without any upward part, so walls and steep slopes can't be climbed by sliding.
 */
[[nodiscard]] inline auto slide_along(const math::vector3& remaining, const math::vector3& normal, std::float_t cos_slope_limit) -> math::vector3 {
  auto slid = remaining - normal * math::vector3::dot(remaining, normal);

  if (!is_walkable(normal, cos_slope_limit)) {
    const auto upward = math::vector3::dot(slid, math::vector3::up);

    if (upward > 0.0f) {
      slid = slid - math::vector3::up * upward;
    }
  }

  return slid;
}

/** @brief One surface a move_character call ran into, for OnControllerColliderHit. */
struct controller_hit {
  scenes::node controller{};
  scenes::node other{};
  math::vector3 point{math::vector3::zero};
  math::vector3 normal{math::vector3::up};
  math::vector3 move_direction{math::vector3::zero};
  std::float_t move_length{0.0f};
}; // struct controller_hit

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_CHARACTER_CONTROLLER_HPP_
