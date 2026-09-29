// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/preferences_panel.hpp>

#include <string>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/reflection/enum.hpp>

#include <editor/editor_module.hpp>

namespace editor {

auto preferences_panel::draw([[maybe_unused]] editor_state& state) -> void {
  if (!is_open) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2{420.0f, 0.0f}, ImGuiCond_FirstUseEver);

  if (!ImGui::Begin(window_name, &is_open)) {
    ImGui::End();
    return;
  }

  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();
  auto& preferences = editor_module.preferences();
  auto changed = false;

  ImGui::SeparatorText("Colors");

  if (ImGui::BeginCombo("Axis Colors", std::string{sbx::reflection::to_string(preferences.axis_colors)}.c_str())) {
    for (const auto scheme : sbx::reflection::enum_values<axis_color_scheme>()) {
      if (ImGui::Selectable(std::string{sbx::reflection::to_string(scheme)}.c_str(), scheme == preferences.axis_colors)) {
        preferences.axis_colors = scheme;
        changed = true;
      }
    }

    ImGui::EndCombo();
  }

  ImGui::SeparatorText("Snapping (hold Ctrl while dragging a gizmo)");

  ImGui::DragFloat("Move Step", &preferences.translate_snap, 0.01f, 0.001f, 100.0f, "%.3f");
  changed |= ImGui::IsItemDeactivatedAfterEdit();
  ImGui::DragFloat("Rotate Step", &preferences.rotate_snap, 0.5f, 0.1f, 180.0f, "%.1f deg");
  changed |= ImGui::IsItemDeactivatedAfterEdit();
  ImGui::DragFloat("Scale Step", &preferences.scale_snap, 0.005f, 0.001f, 10.0f, "%.3f");
  changed |= ImGui::IsItemDeactivatedAfterEdit();

  if (changed) {
    editor_module.save_preferences();
  }

  // Saved with the camera's own camera.yaml when the editor closes -- see editor_camera.
  ImGui::SeparatorText("Editor Camera");

  auto& camera = editor_module.editor_camera();

  auto move_speed = camera.move_speed();

  if (ImGui::DragFloat("Move Speed", &move_speed, 0.1f, 0.1f, 500.0f, "%.1f")) {
    camera.set_move_speed(move_speed);
  }

  auto look_sensitivity = camera.look_sensitivity();

  if (ImGui::DragFloat("Look Sensitivity", &look_sensitivity, 0.0001f, 0.0001f, 0.05f, "%.4f")) {
    camera.set_look_sensitivity(look_sensitivity);
  }

  ImGui::DragFloat("Field of View", &camera.params().fov_degrees, 0.5f, 10.0f, 150.0f, "%.1f deg");
  ImGui::DragFloat("Near Plane", &camera.params().near_plane, 0.01f, 0.001f, camera.params().far_plane, "%.3f");
  ImGui::DragFloat("Far Plane", &camera.params().far_plane, 1.0f, camera.params().near_plane, 100000.0f, "%.1f");

  ImGui::End();
}

} // namespace editor
