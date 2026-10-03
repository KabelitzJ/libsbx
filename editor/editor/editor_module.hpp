// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_MODULE_HPP_
#define EDITOR_EDITOR_MODULE_HPP_

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>
#include <libsbx/render/ui/ui_module.hpp>

#include <editor/editor_camera.hpp>
#include <editor/editor_preferences.hpp>
#include <editor/editor_ui_layer.hpp>
#include <editor/play_mode_controller.hpp>
#include <editor/viewport_camera.hpp>

namespace editor {

/** @brief The editor's module: owns editor_ui_layer, registers it with ui_module, and does one-time engine setup. */
class editor_module final : public sbx::utility::noncopyable {

public:

  using dependencies = sbx::core::dependency_list<sbx::graphics::graphics_module, sbx::assets::assets_module, sbx::scenes::scenes_module, sbx::render::scene_renderer_module, sbx::render::ui_module>;

  editor_module();

  ~editor_module();

  /** @copydoc editor_ui_layer::is_viewport_hovered */
  [[nodiscard]] auto is_viewport_hovered() const noexcept -> bool {
    return _ui_layer.is_viewport_hovered();
  }

  /** @copydoc editor_ui_layer::set_scene_path */
  auto set_scene_path(std::filesystem::path path) -> void {
    _ui_layer.set_scene_path(std::move(path));
  }

  /** @copydoc editor_ui_layer::request_quit */
  auto request_quit() -> void {
    _ui_layer.request_quit();
  }

  /** @copydoc editor_ui_layer::new_scene */
  auto new_scene() -> void {
    _ui_layer.new_scene();
  }

  /** @copydoc editor_ui_layer::open_scene */
  auto open_scene(const std::filesystem::path& path) -> void {
    _ui_layer.open_scene(path);
  }

  /**
   * @brief The current play state.
   *
   * @return Edit, playing or paused.
   */
  [[nodiscard]] auto play_state() const noexcept -> editor::play_state {
    return _play_mode.state();
  }

  /** @brief Enters Play mode and switches the viewport to the play camera immediately, so no stale editor-camera frame shows. */
  auto enter_play_mode() -> void;

  /** @brief Exits Play mode and switches the viewport back to the editor camera immediately. */
  auto exit_play_mode() -> void;

  auto toggle_pause() -> void {
    _play_mode.toggle_pause();
  }

  /**
   * @brief The editor's free-fly viewport camera, driven by application::update().
   *
   * @return The camera.
   */
  [[nodiscard]] auto editor_camera() noexcept -> editor::editor_camera& {
    return _editor_camera;
  }

  [[nodiscard]] auto preferences() noexcept -> editor_preferences& {
    return _preferences;
  }

  /** @brief Writes preferences() to disk; Edit > Preferences calls it after every change. */
  auto save_preferences() const -> void {
    _preferences.save(_preferences_path());
  }

  /**
   * @brief The camera the viewport renders, picks and draws gizmos through: the editor camera in Edit mode, otherwise the scene's active camera.
   *
   * @param scene The active scene.
   *
   * @return The camera pose, or nullopt without a camera.
   */
  [[nodiscard]] auto viewport_camera(sbx::scenes::scene& scene) const -> std::optional<editor::viewport_camera_pose>;

private:

  [[nodiscard]] auto _camera_state_path() const -> std::filesystem::path;

  [[nodiscard]] auto _preferences_path() const -> std::filesystem::path;

  std::string _ini_file;
  editor::editor_camera _editor_camera;
  editor_preferences _preferences;
  editor_ui_layer _ui_layer{};
  play_mode_controller _play_mode{};

}; // class editor_module

} // namespace editor

#endif // EDITOR_EDITOR_MODULE_HPP_
