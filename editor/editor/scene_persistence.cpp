// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_ui_layer.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/scenes/scene_serializer.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <editor/widgets/text_field.hpp>

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

auto editor_ui_layer::_draw_save_as_dialog() -> void {
  if (_show_save_as_dialog) {
    ImGui::OpenPopup("Save Scene As");
    _show_save_as_dialog = false;
  }

  if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextDisabled("Relative to the project's assets directory.");
    widgets::draw_text_field<256u>("##save_as_path", _save_as_path);

    if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
      if (!_save_as_path.empty()) {
        _save_scene(std::filesystem::path{_save_as_path});

        if (_quit_after_save_as) {
          _quit_after_save_as = false;
          sbx::core::engine::quit();
        }

        ImGui::CloseCurrentPopup();
      }
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
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
        _save_as_path = "scenes/new_scene.yaml";
        _show_save_as_dialog = true;
        _quit_after_save_as = true;
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
