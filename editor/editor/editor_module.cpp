// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_module.hpp>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/physics/physics_module.hpp>

namespace editor {

editor_module::editor_module()
: _ini_file{(sbx::core::engine::project().root() / ".sbx" / "editor" / "imgui.ini").string()},
  _editor_camera{editor::editor_camera::load(_camera_state_path())},
  _preferences{editor_preferences::load(_preferences_path())} {
  std::filesystem::create_directories(std::filesystem::path{_ini_file}.parent_path());

  ImGui::GetIO().IniFilename = nullptr;

  ImGui::LoadIniSettingsFromDisk(_ini_file.data());

  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();
  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
  auto& physics_module = sbx::core::engine::get_module<sbx::physics::physics_module>();

  ui_module.add_layer(&_ui_layer);
  scene_renderer_module.set_grid_enabled(true);
  physics_module.set_debug_draw_flags(sbx::physics::debug_draw_flags{});
}

editor_module::~editor_module() {
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  ui_module.remove_layer(&_ui_layer);

  ImGui::SaveIniSettingsToDisk(_ini_file.c_str());

  _editor_camera.save(_camera_state_path());
}

auto editor_module::enter_play_mode() -> void {
  _play_mode.enter_play_mode();

  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();

  scene_renderer_module.set_camera_override(std::nullopt);
  scene_renderer_module.set_grid_enabled(false);
}

auto editor_module::exit_play_mode() -> void {
  _play_mode.exit_play_mode();

  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();

  scene_renderer_module.set_camera_override(edit_camera_data(sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene()));
  scene_renderer_module.set_grid_enabled(true);

  // The reload above rebuilds the whole registry, so any command pushed during the play session
  // now references stale entities/state — drop them. Entering Play does not clear (@ref
  // play_mode_controller), since it never touches node identity or the registry.
  _ui_layer.clear_command_stack();
}

auto editor_module::preview_camera_node(sbx::scenes::scene& scene) const -> sbx::scenes::node {
  if (_preview_camera == sbx::math::uuid::nil()) {
    return sbx::scenes::node{};
  }

  auto node = scene.find(_preview_camera);

  return node.is_valid() && node.has_component<sbx::scenes::camera>() ? node : sbx::scenes::node{};
}

auto editor_module::edit_camera_data(sbx::scenes::scene& scene) const -> sbx::render::camera_data {
  auto node = preview_camera_node(scene);

  if (!node.is_valid()) {
    return _editor_camera.to_camera_data();
  }

  // The same fields the renderer reads from the scene's own camera in Play.
  const auto& camera = node.get_component<sbx::scenes::camera>();
  const auto& world = node.get_component<sbx::scenes::world_transform>();

  auto data = sbx::render::camera_data{};
  data.view = world.inverse;
  data.position = sbx::math::vector3{world.matrix[3]};
  data.fov_degrees = camera.fov_degrees;
  data.near_plane = camera.near_plane;
  data.far_plane = camera.far_plane;
  data.post_process = camera.post_process;
  data.is_active = true;

  return data;
}

auto editor_module::viewport_camera(sbx::scenes::scene& scene) const -> std::optional<editor::viewport_camera_pose> {
  if (_play_mode.state() == play_state::edit) {
    // Picking and gizmos go through whatever the Viewport shows.
    if (auto node = preview_camera_node(scene); node.is_valid()) {
      return editor::viewport_camera_pose{node.world_matrix(), node.get_component<sbx::scenes::camera>()};
    }

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

auto editor_module::_preferences_path() const -> std::filesystem::path {
  return sbx::core::engine::project().root() / ".sbx" / "editor" / "preferences.yaml";
}

} // namespace editor
