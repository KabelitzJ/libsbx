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
 * @brief The editor's free-fly viewport camera: not a scene node, so never saved into scenes or the play snapshot. Persisted per project in `.sbx/editor/camera.yaml`.
 *
 * WASD moves, Q/E go down/up, right mouse looks, Shift sprints.
 */
class editor_camera {

public:

  editor_camera() = default;

  /** @brief Applies movement and look from input; the caller decides when (right mouse engaged). */
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

  /**
   * @brief The camera's world matrix; it has no parent, so local is world.
   *
   * @return The world matrix.
   */
  [[nodiscard]] auto world_matrix() const -> sbx::math::matrix4x4 {
    return _transform.matrix();
  }

  /**
   * @brief The camera as scene_renderer_module renders it.
   *
   * @return The camera data.
   */
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

  /**
   * @brief Loads the camera, defaulting missing fields; a missing file means all defaults.
   *
   * @param path The camera file.
   *
   * @return The camera.
   */
  [[nodiscard]] static auto load(const std::filesystem::path& path) -> editor_camera;

  auto save(const std::filesystem::path& path) const -> void;

private:

  auto _sync_angles_from_rotation() -> void;

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
