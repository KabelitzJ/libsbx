// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/history_panel.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/scenes_module.hpp>

namespace editor {

auto history_panel::draw(editor_state& state) -> void {
  SBX_PROFILE_SCOPE("history_panel::draw");

  if (!is_open) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2{280.0f, 360.0f}, ImGuiCond_FirstUseEver);

  if (!ImGui::Begin(window_name, &is_open)) {
    ImGui::End();
    return;
  }

  auto applied = std::size_t{0u};
  const auto labels = state.commands.history(applied);

  // Row 0 is the state before any entry; row i is "after entry i".
  auto target = std::optional<std::size_t>{};

  if (ImGui::Selectable(ICON_MDI_FILE_OUTLINE " Initial State", applied == 0u)) {
    target = 0u;
  }

  for (auto index = std::size_t{0u}; index < labels.size(); ++index) {
    ImGui::PushID(static_cast<int>(index));

    const auto is_applied = index < applied;

    // Redoable entries are greyed, like Godot's History dock.
    if (!is_applied) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    }

    if (ImGui::Selectable(labels[index].c_str(), index + 1u == applied)) {
      target = index + 1u;
    }

    if (!is_applied) {
      ImGui::PopStyleColor();
    }

    ImGui::PopID();
  }

  if (labels.empty()) {
    ImGui::TextDisabled("Nothing to undo yet.");
  }

  ImGui::End();

  if (!target || *target == applied) {
    return;
  }

  auto& scene = sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();

  for (auto count = applied; count > *target; --count) {
    state.undo(scene);
  }

  for (auto count = applied; count < *target; ++count) {
    state.redo(scene);
  }
}

} // namespace editor
