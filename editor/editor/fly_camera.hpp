// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_FLY_CAMERA_HPP_
#define EDITOR_FLY_CAMERA_HPP_

#include <algorithm>
#include <cmath>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/angle.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/input.hpp>

#include <libsbx/scenes/scene.hpp>

namespace editor {

/**
 * @brief Free-fly viewport camera. WASD to move, Q/E down/up, hold right mouse to look, shift to
 * sprint. Drives a scene camera node's transform. Not part of the scene — dev/editor tooling.
 */
class fly_camera {

public:

  fly_camera() = default;

  explicit fly_camera(sbx::scenes::scene::node node);

  auto update() -> void;

  auto set_move_speed(std::float_t speed) -> void;

  auto set_look_sensitivity(std::float_t sensitivity) -> void;

private:

  sbx::scenes::scene::node _node{};
  std::float_t _yaw{0.0f};
  std::float_t _pitch{0.0f};
  std::float_t _move_speed{4.0f};
  std::float_t _look_sensitivity{0.0025f};
  sbx::math::vector2 _last_mouse{0.0f, 0.0f};
  bool _is_looking{false};

}; // class fly_camera

} // namespace editor

#endif // EDITOR_FLY_CAMERA_HPP_
