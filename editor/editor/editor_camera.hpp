// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_CAMERA_HPP_
#define EDITOR_EDITOR_CAMERA_HPP_

#include <cmath>
#include <filesystem>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector2.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/render/render_packet.hpp>

namespace editor {

/**
 * @brief The editor's own free-fly viewport camera.
 *
 * Not a scenes::node/ECS entity — never written into a scene file and never swept up in
 * play_mode_controller's snapshot/restore. Persists per-project at
 * `<project_root>/.sbx/editor/camera.yaml` (see load()/save()). Reuses scenes::local_transform
 * and scenes::camera as plain data, with no ECS association. Owns its own free-fly input (WASD
 * move, Q/E down/up, hold right mouse to look, shift to sprint).
 */
class editor_camera {

public:

  editor_camera() = default;

  /** @brief Drives movement/look from input. Caller decides when to call this — see application::update()'s right-mouse-engage gating. */
  auto update() -> void;

  [[nodiscard]] auto transform() noexcept -> sbx::scenes::local_transform& {
    return _transform;
  }

  [[nodiscard]] auto transform() const noexcept -> const sbx::scenes::local_transform& {
    return _transform;
  }

  [[nodiscard]] auto params() noexcept -> sbx::scenes::camera& {
    return _params;
  }

  [[nodiscard]] auto params() const noexcept -> const sbx::scenes::camera& {
    return _params;
  }

  /** @brief No parent to fold in — this camera is never part of the scene hierarchy, so local == world. */
  [[nodiscard]] auto world_matrix() const -> sbx::math::matrix4x4 {
    return _transform.matrix();
  }

  /** @brief This camera's pose/params in the shape scene_renderer_module renders from — see scene_renderer_module::set_camera_override. */
  [[nodiscard]] auto to_camera_data() const -> sbx::render::camera_data;

  auto set_move_speed(std::float_t speed) -> void {
    _move_speed = speed;
  }

  auto set_look_sensitivity(std::float_t sensitivity) -> void {
    _look_sensitivity = sensitivity;
  }

  [[nodiscard]] auto move_speed() const noexcept -> std::float_t {
    return _move_speed;
  }

  [[nodiscard]] auto look_sensitivity() const noexcept -> std::float_t {
    return _look_sensitivity;
  }

  /** @brief Loads from path, defaulting any/all fields not present. A missing file just means all defaults — expected on a project's first launch, not an error. */
  [[nodiscard]] static auto load(const std::filesystem::path& path) -> editor_camera;

  auto save(const std::filesystem::path& path) const -> void;

private:

  sbx::scenes::local_transform _transform{};
  sbx::scenes::camera _params{};

  std::float_t _yaw{0.0f};
  std::float_t _pitch{0.0f};
  std::float_t _move_speed{4.0f};
  std::float_t _look_sensitivity{0.0025f};
  sbx::math::vector2 _last_mouse{0.0f, 0.0f};
  bool _is_looking{false};

}; // class editor_camera

} // namespace editor

#endif // EDITOR_EDITOR_CAMERA_HPP_
