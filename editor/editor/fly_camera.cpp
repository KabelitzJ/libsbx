// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/fly_camera.hpp>

namespace editor {

fly_camera::fly_camera(const sbx::scenes::node& node)
: _node{node} { }

auto fly_camera::update() -> void {
  if (!_node.is_valid()) {
    return;
  }

  const auto delta_time = sbx::core::engine::delta_time();

  const auto mouse = sbx::platform::input::mouse_position();

  if (sbx::platform::input::is_mouse_button_pressed(sbx::platform::mouse_button::right)) {
    _is_looking = true;
    _last_mouse = mouse; // reset so the first look frame has no delta
  }

  if (sbx::platform::input::is_mouse_button_released(sbx::platform::mouse_button::right)) {
    _is_looking = false;
  }

  if (_is_looking) {
    const auto delta = mouse - _last_mouse;
    _last_mouse = mouse;

    _yaw -= static_cast<std::float_t>(delta.x()) * _look_sensitivity;
    _pitch -= static_cast<std::float_t>(delta.y()) * _look_sensitivity;

    const auto limit = sbx::math::to_radians(sbx::math::degree{89.0f}).value();
    _pitch = std::clamp(_pitch, -limit, limit);
  }

  const auto yaw_rotation = sbx::math::quaternion{sbx::math::vector3f{0.0f, 1.0f, 0.0f}, sbx::math::angle{sbx::math::radian{_yaw}}};
  const auto pitch_rotation = sbx::math::quaternion{sbx::math::vector3f{1.0f, 0.0f, 0.0f}, sbx::math::angle{sbx::math::radian{_pitch}}};
  const auto rotation = yaw_rotation * pitch_rotation;

  const auto forward = rotation * sbx::math::vector3f{0.0f, 0.0f, -1.0f};
  const auto right = rotation * sbx::math::vector3f{1.0f, 0.0f, 0.0f};
  const auto up = sbx::math::vector3f{0.0f, 1.0f, 0.0f};

  auto direction = sbx::math::vector3f{0.0f, 0.0f, 0.0f};

  if (sbx::platform::input::is_key_down(sbx::platform::key::w)) { direction += forward; }
  if (sbx::platform::input::is_key_down(sbx::platform::key::s)) { direction -= forward; }
  if (sbx::platform::input::is_key_down(sbx::platform::key::d)) { direction += right; }
  if (sbx::platform::input::is_key_down(sbx::platform::key::a)) { direction -= right; }
  if (sbx::platform::input::is_key_down(sbx::platform::key::e)) { direction += up; }
  if (sbx::platform::input::is_key_down(sbx::platform::key::q)) { direction -= up; }

  auto speed = _move_speed;

  if (sbx::platform::input::is_key_down(sbx::platform::key::left_shift)) {
    speed *= 4.0f;
  }

  auto& transform = _node.transform();

  if (direction.length_squared() > 0.0f) {
    transform.position += sbx::math::vector3f::normalized(direction) * speed * delta_time.value();
  }

  transform.rotation = rotation;
}

auto fly_camera::set_move_speed(std::float_t speed) -> void { 
  _move_speed = speed; 
}

auto fly_camera::set_look_sensitivity(std::float_t sensitivity) -> void { 
  _look_sensitivity = sensitivity; 
}

} // namespace editor
