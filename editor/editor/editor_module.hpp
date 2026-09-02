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
#include <editor/editor_ui_layer.hpp>
#include <editor/play_mode_controller.hpp>
#include <editor/viewport_camera.hpp>

namespace editor {

/**
 * @brief The editor's core::module — lifecycle only. Owns editor_ui_layer (see there for
 * everything ImGui-related: dockspace, panels, the viewport, scene save/quit dialogs) and registers
 * it with ui_module; the only things left here are IniFilename (needs the ImGui context, which
 * exists by construction time, but is otherwise not a "what does the UI draw" concern) and
 * grid_enabled, both one-time engine-level settings rather than per-frame UI.
 */
class editor_module final : public sbx::utility::noncopyable {

public:

  using dependencies = sbx::core::dependency_list<sbx::graphics::graphics_module, sbx::assets::assets_module, sbx::scenes::scenes_module, sbx::render::scene_renderer_module, sbx::render::ui_module>;

  editor_module();

  ~editor_module();

  /** @see editor_ui_layer::is_viewport_hovered */
  [[nodiscard]] auto is_viewport_hovered() const noexcept -> bool {
    return _ui_layer.is_viewport_hovered();
  }

  /** @brief Called once by application.cpp right after its own initial scene load. */
  auto set_scene_path(std::filesystem::path path) -> void {
    _ui_layer.set_scene_path(std::move(path));
  }

  /** @see editor_ui_layer::request_quit */
  auto request_quit() -> void {
    _ui_layer.request_quit();
  }

  /** @see play_mode_controller */
  [[nodiscard]] auto play_state() const noexcept -> editor::play_state {
    return _play_mode.state();
  }

  /**
   * @brief Enters Play mode and immediately hands the viewport over to the scene's own play
   * camera — pushes the render override itself rather than waiting for application::update()'s
   * next per-frame poll, so there's no stale editor-camera frame while Play spins up.
   */
  auto enter_play_mode() -> void;

  /**
   * @brief Exits Play mode and immediately switches the viewport back to the editor camera — see
   * enter_play_mode()'s doc comment on why this pushes the override itself instead of waiting.
   */
  auto exit_play_mode() -> void;

  auto toggle_pause() -> void {
    _play_mode.toggle_pause();
  }

  /** @brief The editor's own free-fly viewport camera — see editor_camera's doc comment. Mutable so application::update() can drive it from input. */
  [[nodiscard]] auto editor_camera() noexcept -> editor::editor_camera& {
    return _editor_camera;
  }

  /**
   * @brief The world matrix + camera params the viewport should currently render/pick/gizmo
   * through: the editor camera while play_state()==edit, otherwise the scene's own active (play)
   * camera if it has one. Shared by scene_renderer_module's per-frame override (application.cpp),
   * viewport picking, and the gizmo so all three always agree on what's actually on screen.
   */
  [[nodiscard]] auto viewport_camera(sbx::scenes::scene& scene) const -> std::optional<editor::viewport_camera_pose>;

private:

  [[nodiscard]] auto _camera_state_path() const -> std::filesystem::path;

  std::string _ini_file;
  editor::editor_camera _editor_camera;
  editor_ui_layer _ui_layer{};
  play_mode_controller _play_mode{};

}; // class editor_module

} // namespace editor

#endif // EDITOR_EDITOR_MODULE_HPP_
