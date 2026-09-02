// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_camera.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/angle.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/input.hpp>

namespace editor {

auto editor_camera::update() -> void {
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

  if (direction.length_squared() > 0.0f) {
    _transform.position += sbx::math::vector3f::normalized(direction) * speed * delta_time.value();
  }

  _transform.rotation = rotation;
}

auto editor_camera::to_camera_data() const -> sbx::render::camera_data {
  auto data = sbx::render::camera_data{};

  data.view = sbx::math::matrix4x4::inverted(world_matrix());
  data.position = _transform.position;
  data.fov_degrees = _params.fov_degrees;
  data.near_plane = _params.near_plane;
  data.far_plane = _params.far_plane;
  data.exposure = _params.exposure;
  data.is_active = true;

  return data;
}

auto editor_camera::load(const std::filesystem::path& path) -> editor_camera {
  auto result = editor_camera{};

  if (!std::filesystem::exists(path)) {
    return result;
  }

  const auto root = YAML::LoadFile(path.string());

  if (root["position"]) {
    result._transform.position = root["position"].as<sbx::math::vector3f>();
  }

  if (root["rotation"]) {
    result._transform.rotation = root["rotation"].as<sbx::math::quaternion>();
  }

  if (root["fov_degrees"]) {
    result._params.fov_degrees = root["fov_degrees"].as<std::float_t>();
  }

  if (root["near_plane"]) {
    result._params.near_plane = root["near_plane"].as<std::float_t>();
  }

  if (root["far_plane"]) {
    result._params.far_plane = root["far_plane"].as<std::float_t>();
  }

  if (root["exposure"]) {
    result._params.exposure = root["exposure"].as<std::float_t>();
  }

  if (root["move_speed"]) {
    result._move_speed = root["move_speed"].as<std::float_t>();
  }

  if (root["look_sensitivity"]) {
    result._look_sensitivity = root["look_sensitivity"].as<std::float_t>();
  }

  return result;
}

auto editor_camera::save(const std::filesystem::path& path) const -> void {
  auto root = YAML::Node{};

  root["position"] = _transform.position;
  root["rotation"] = _transform.rotation;
  root["fov_degrees"] = _params.fov_degrees;
  root["near_plane"] = _params.near_plane;
  root["far_plane"] = _params.far_plane;
  root["exposure"] = _params.exposure;
  root["move_speed"] = _move_speed;
  root["look_sensitivity"] = _look_sensitivity;

  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }

  auto out = std::ofstream{path};
  out << root;
}

} // namespace editor
