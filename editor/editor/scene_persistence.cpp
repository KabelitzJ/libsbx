// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_ui_layer.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene_serializer.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace editor {

auto editor_ui_layer::request_quit() -> void {
  _request_discard_confirmation(pending_scene_action::quit, {});
}

auto editor_ui_layer::new_scene() -> void {
  _request_discard_confirmation(pending_scene_action::new_scene, {});
}

auto editor_ui_layer::open_scene(const std::filesystem::path& path) -> void {
  _request_discard_confirmation(pending_scene_action::open_scene, path);
}

auto editor_ui_layer::_request_discard_confirmation(pending_scene_action action, std::filesystem::path path) -> void {
  _pending_scene_action = action;
  _pending_open_path = std::move(path);

  if (_is_scene_dirty()) {
    _show_unsaved_changes_dialog = true;
  } else {
    _run_pending_scene_action();
  }
}

auto editor_ui_layer::_run_pending_scene_action() -> void {
  const auto action = std::exchange(_pending_scene_action, pending_scene_action::none);
  const auto path = std::exchange(_pending_open_path, std::filesystem::path{});

  switch (action) {
    case pending_scene_action::none: {
      break;
    }
    case pending_scene_action::quit: {
      sbx::core::engine::quit();
      break;
    }
    case pending_scene_action::new_scene: {
      _new_scene();
      break;
    }
    case pending_scene_action::open_scene: {
      _open_scene(path);
      break;
    }
  }
}

auto editor_ui_layer::_save_scene(const std::filesystem::path& path) -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  auto& scene = scenes_module.active_scene();

  auto handle = assets_module.create_scene(sbx::scenes::scene_serializer::build(scene), scene.name());
  assets_module.save_scene(handle, path);

  _scene_path = path;
}

auto editor_ui_layer::_is_scene_dirty() -> bool {
  if (_scene_path.empty()) {
    return true; // never saved
  }

  auto& project = sbx::core::engine::project();
  auto file = std::ifstream{project.assets_directory() / _scene_path, std::ios::binary};

  if (!file) {
    return true; // no file there yet
  }

  const auto on_disk = std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  return on_disk != sbx::scenes::scene_serializer::serialize(scenes_module.active_scene());
}

auto editor_ui_layer::_open_save_as_dialog() -> void {
  auto& project = sbx::core::engine::project();

  const auto start_dir = _scene_path.empty() ? project.assets_directory() : (project.assets_directory() / _scene_path).parent_path();
  const auto default_name = _scene_path.empty() ? std::string{"new_scene.scene"} : _scene_path.filename().string();

  _save_dialog.open({
    .title = "Save Scene As",
    .mode = sbx::render::file_dialog_mode::save_file,
    .start_dir = start_dir,
    .extensions = {".scene"},
    .shortcuts = {{.label = "Assets", .path = project.assets_directory()}},
    .default_file_name = default_name,
  });
}

auto editor_ui_layer::_draw_save_as_dialog() -> void {
  _save_dialog.draw();

  auto picked = _save_dialog.result();

  if (!picked) {
    return;
  }

  if (picked->empty()) {
    // Cancelled: don't run the pending action on some unrelated later save.
    _run_pending_after_save_as = false;
    return;
  }

  auto& project = sbx::core::engine::project();

  auto ec = std::error_code{};
  const auto relative = std::filesystem::relative(picked->front(), project.assets_directory(), ec);

  // Relative to the assets directory when the pick is inside it, otherwise absolute; save_scene accepts both.
  const auto scene_path = (!ec && !relative.empty() && relative.begin()->string() != "..") ? relative : picked->front();

  _save_scene(scene_path);

  if (_run_pending_after_save_as) {
    _run_pending_after_save_as = false;
    _run_pending_scene_action();
  }
}

auto editor_ui_layer::_draw_unsaved_changes_dialog() -> void {
  if (_show_unsaved_changes_dialog) {
    ImGui::OpenPopup("Unsaved Changes");
    _show_unsaved_changes_dialog = false;
  }

  if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(ICON_MDI_CONTENT_SAVE_ALERT " The current scene has unsaved changes.");

    if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
      if (_scene_path.empty()) {
        _run_pending_after_save_as = true;
        _open_save_as_dialog();
      } else {
        _save_scene(_scene_path);
        _run_pending_scene_action();
      }

      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Don't Save")) {
      ImGui::CloseCurrentPopup();
      _run_pending_scene_action();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
      _pending_scene_action = pending_scene_action::none;
    }

    ImGui::EndPopup();
  }
}

auto editor_ui_layer::_new_scene() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  scenes_module.active_scene() = sbx::scenes::scene{};

  auto& scene = scenes_module.active_scene();

  // The same default content as application.cpp's startup fallback: a play camera owned by the scene.
  auto camera = scene.create_node("Camera");
  camera.add_component<sbx::scenes::camera>();
  scene.set_active_camera(camera);

  auto& skybox = camera.add_component<sbx::scenes::skybox>();
  skybox.environment = assets_module.load_environment_map("environments/sky.hdr");
  skybox.intensity = 1.0f;

  _scene_path.clear();
  _state.clear_selection();
  _state.clear_command_stack();
}

auto editor_ui_layer::_open_scene(const std::filesystem::path& path) -> void {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto handle = assets_module.load_scene(path);

  if (!handle.is_valid()) {
    return;
  }

  sbx::scenes::scene_serializer::load(scenes_module.active_scene(), handle->snapshot());

  _scene_path = path;
  _state.clear_selection();
  _state.clear_command_stack();
}

auto editor_ui_layer::_open_open_scene_dialog() -> void {
  auto& project = sbx::core::engine::project();

  _open_dialog.open({
    .title = "Open Scene",
    .mode = sbx::render::file_dialog_mode::open_file,
    .start_dir = project.assets_directory(),
    .extensions = {".scene"},
    .shortcuts = {{.label = "Assets", .path = project.assets_directory()}},
  });
}

auto editor_ui_layer::_draw_open_scene_dialog() -> void {
  _open_dialog.draw();

  auto picked = _open_dialog.result();

  if (!picked || picked->empty()) {
    return;
  }

  auto& project = sbx::core::engine::project();

  auto ec = std::error_code{};
  const auto relative = std::filesystem::relative(picked->front(), project.assets_directory(), ec);

  const auto scene_path = (!ec && !relative.empty() && relative.begin()->string() != "..") ? relative : picked->front();

  open_scene(scene_path); // re-enters the unsaved-changes guard in case the scene changed meanwhile
}

} // namespace editor
