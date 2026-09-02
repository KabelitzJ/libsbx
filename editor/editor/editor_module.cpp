// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_module.hpp>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/physics/physics_module.hpp>

namespace editor {

editor_module::editor_module()
: _ini_file{(sbx::core::engine::project().root() / ".sbx" / "editor" / "imgui.ini").string()},
  _editor_camera{editor::editor_camera::load(_camera_state_path())} {
  std::filesystem::create_directories(std::filesystem::path{_ini_file}.parent_path());

  ImGui::GetIO().IniFilename = nullptr;

  ImGui::LoadIniSettingsFromDisk(_ini_file.data());

  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();
  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
  auto& physics_module = sbx::core::engine::get_module<sbx::physics::physics_module>();

  ui_module.add_layer(&_ui_layer);
  scene_renderer_module.set_grid_enabled(true);
  physics_module.set_debug_draw_flags(sbx::physics::debug_draw_flags{.colliders = true});
}

editor_module::~editor_module() {
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  ui_module.remove_layer(&_ui_layer);

  ImGui::SaveIniSettingsToDisk(_ini_file.c_str());

  _editor_camera.save(_camera_state_path());
}

auto editor_module::enter_play_mode() -> void {
  _play_mode.enter_play_mode();

  sbx::core::engine::get_module<sbx::render::scene_renderer_module>().set_camera_override(std::nullopt);
}

auto editor_module::exit_play_mode() -> void {
  _play_mode.exit_play_mode();

  sbx::core::engine::get_module<sbx::render::scene_renderer_module>().set_camera_override(_editor_camera.to_camera_data());

  // The reload above rebuilds the whole registry — any command pushed during the play session
  // referenced entities/state that no longer exists in a replayable way, so drop them. Entering
  // Play deliberately does NOT clear (see play_mode_controller.hpp's doc comment): it never
  // touches node identity or the registry, so pre-Play history stays perfectly valid while playing.
  _ui_layer.clear_command_stack();
}

auto editor_module::viewport_camera(sbx::scenes::scene& scene) const -> std::optional<editor::viewport_camera_pose> {
  if (_play_mode.state() == play_state::edit) {
    return editor::viewport_camera_pose{_editor_camera.world_matrix(), _editor_camera.params()};
  }

  if (!scene.has_active_camera()) {
    return std::nullopt;
  }

  auto camera_node = scene.active_camera();

  if (!camera_node.is_valid() || !camera_node.has_component<sbx::scenes::camera>()) {
    return std::nullopt;
  }

  return editor::viewport_camera_pose{camera_node.world_matrix(), camera_node.get_component<sbx::scenes::camera>()};
}

auto editor_module::_camera_state_path() const -> std::filesystem::path {
  return sbx::core::engine::project().root() / ".sbx" / "editor" / "camera.yaml";
}

} // namespace editor
