// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/asset_browser_panel.hpp>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <vector>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

namespace editor {

// Recursively lists subdirectories only, live per expanded node — cheap (names only, no imports).
auto asset_browser_panel::_draw_directory_tree(editor_state& state, const std::filesystem::path& absolute_assets_root, const std::filesystem::path& relative_directory) -> void {
  const auto absolute_directory = absolute_assets_root / relative_directory;

  if (!std::filesystem::exists(absolute_directory)) {
    return;
  }

  auto subdirectories = std::vector<std::filesystem::path>{};

  for (const auto& dir_entry : std::filesystem::directory_iterator{absolute_directory}) {
    if (dir_entry.is_directory() && !is_hidden_from_browser(dir_entry.path().filename(), true)) {
      subdirectories.push_back(dir_entry.path());
    }
  }

  std::ranges::sort(subdirectories, filename_less);

  for (const auto& subdirectory : subdirectories) {
    const auto relative_child = relative_directory / subdirectory.filename();
    const auto child_name = subdirectory.filename().string();
    const auto is_leaf = !has_visible_subdirectories(subdirectory);
    const auto is_renaming_this = relative_child == _renaming_path;

    auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (_current_directory == relative_child) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (is_leaf) {
      flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    ImGui::PushID(relative_child.string().c_str());

    if (_pending_reveal && is_ancestor_or_self(relative_child, *_pending_reveal)) {
      ImGui::SetNextItemOpen(true);
    }

    const auto is_open = is_renaming_this
      ? ImGui::TreeNodeEx("##dir", flags, "%s", ICON_MDI_FOLDER)
      : ImGui::TreeNodeEx("##dir", flags, "%s %s", ICON_MDI_FOLDER, child_name.c_str());

    if (_pending_reveal && relative_child == *_pending_reveal) {
      ImGui::SetScrollHereY(0.5f);
    }

    if (!is_renaming_this) {
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        _navigate_to(relative_child);
      }

      const auto move_payload = make_move_drag_payload(relative_child);

      if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(asset_move_drag_payload_type, &move_payload, sizeof(move_payload));
        ImGui::TextUnformatted(child_name.c_str());
        ImGui::EndDragDropSource();
      }
    }

    _draw_move_drop_target(state, relative_child);

    if (is_renaming_this) {
      ImGui::SameLine();
      _draw_rename_field(state, std::numeric_limits<std::float_t>::lowest());
    }

    if (ImGui::BeginPopupContextItem("##dir_context")) {
      _draw_entry_context_menu(state, asset_browser_entry{.path = relative_child, .is_directory = true});
      ImGui::EndPopup();
    }

    if (is_open && !is_leaf) {
      _draw_directory_tree(state, absolute_assets_root, relative_child);
      ImGui::TreePop();
    }

    ImGui::PopID();
  }
}

} // namespace editor
