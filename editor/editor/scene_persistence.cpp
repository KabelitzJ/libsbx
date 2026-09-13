// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_ui_layer.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/scenes/scene_serializer.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace editor {

auto editor_ui_layer::request_quit() -> void {
  if (_is_scene_dirty()) {
    _show_unsaved_changes_dialog = true;
  } else {
    sbx::core::engine::quit();
  }
}

auto editor_ui_layer::_save_scene(const std::filesystem::path& path) -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  sbx::scenes::scene_serializer::save(scenes_module.active_scene(), path);

  _scene_path = path;
}

auto editor_ui_layer::_is_scene_dirty() -> bool {
  if (_scene_path.empty()) {
    return true; // never saved — anything at all counts as unsaved
  }

  auto& project = sbx::core::engine::project();
  auto file = std::ifstream{project.assets_directory() / _scene_path, std::ios::binary};

  if (!file) {
    return true; // no file at that path (yet)
  }

  const auto on_disk = std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  return on_disk != sbx::scenes::scene_serializer::serialize(scenes_module.active_scene());
}

auto editor_ui_layer::_open_save_as_dialog(bool quit_after) -> void {
  auto& project = sbx::core::engine::project();

  _quit_after_save_as = quit_after;

  const auto start_dir = _scene_path.empty() ? project.assets_directory() : (project.assets_directory() / _scene_path).parent_path();
  const auto default_name = _scene_path.empty() ? std::string{"new_scene.yaml"} : _scene_path.filename().string();

  _save_dialog.open({
    .title = "Save Scene As",
    .mode = sbx::render::file_dialog_mode::save_file,
    .start_dir = start_dir,
    .extensions = {".yaml"},
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
    // Cancelled -- don't let a stale "quit once this save-as completes" leak into some unrelated
    // later save (see _quit_after_save_as's doc comment).
    _quit_after_save_as = false;
    return;
  }

  auto& project = sbx::core::engine::project();

  auto ec = std::error_code{};
  const auto relative = std::filesystem::relative(picked->front(), project.assets_directory(), ec);

  // Kept relative to the assets directory (the convention _scene_path documents) whenever the
  // picked location actually resolves under it; left absolute otherwise -- scene_serializer::save
  // accepts either.
  const auto scene_path = (!ec && !relative.empty() && relative.begin()->string() != "..") ? relative : picked->front();

  _save_scene(scene_path);

  if (_quit_after_save_as) {
    _quit_after_save_as = false;
    sbx::core::engine::quit();
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
        _open_save_as_dialog(true);
      } else {
        _save_scene(_scene_path);
        sbx::core::engine::quit();
      }

      _show_unsaved_changes_dialog = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Don't Save")) {
      _show_unsaved_changes_dialog = false;
      ImGui::CloseCurrentPopup();
      sbx::core::engine::quit();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      _show_unsaved_changes_dialog = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

} // namespace editor
