// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/asset_browser_panel.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <editor/commands/scene_commands.hpp>

#include <editor/widgets/inline_rename.hpp>

namespace editor {

auto asset_browser_panel::_refresh_entries() -> void {
  _cached_entries.clear();

  auto& project = sbx::core::engine::project();
  const auto absolute_directory = project.assets_directory() / _current_directory;

  if (!std::filesystem::exists(absolute_directory)) {
    _needs_refresh = false;
    return;
  }

  for (const auto& dir_entry : std::filesystem::directory_iterator{absolute_directory}) {
    if (is_hidden_from_browser(dir_entry.path().filename(), dir_entry.is_directory())) {
      continue;
    }

    auto entry = asset_browser_entry{};
    entry.path = _current_directory / dir_entry.path().filename();
    entry.is_directory = dir_entry.is_directory();

    if (!entry.is_directory) {
      entry.kind = classify_extension(dir_entry.path().extension());
      entry.is_importable = is_importable_kind(entry.kind);
    }

    _cached_entries.push_back(std::move(entry));
  }

  std::ranges::sort(_cached_entries, [](const auto& lhs, const auto& rhs) {
    if (lhs.is_directory != rhs.is_directory) {
      return lhs.is_directory > rhs.is_directory;
    }

    return filename_less(lhs.path, rhs.path);
  });

  _needs_refresh = false;
}

auto asset_browser_panel::_begin_rename(const std::filesystem::path& relative_path, bool is_directory) -> void {
  _renaming_path = relative_path;
  _renaming_is_directory = is_directory;

  const auto name = is_directory ? relative_path.filename().string() : relative_path.stem().string();
  begin_rename(_rename_buffer, _rename_focus_pending, name);
}

auto asset_browser_panel::_draw_rename_field(editor_state& state, std::float_t width) -> void {
  const auto result = draw_rename_field(_rename_buffer, _rename_focus_pending, width);

  if (result.committed) {
    _commit_rename(state);
  }

  if (result.ended) {
    _renaming_path.clear();
  }
}

auto asset_browser_panel::_commit_rename(editor_state& state) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto typed = std::string{_rename_buffer.data()};

  if (typed.empty()) {
    return;
  }

  // Files keep their extension and only the stem is renamed; folders are renamed in full.
  const auto new_name = _renaming_is_directory ? typed : typed + _renaming_path.extension().string();
  const auto new_relative = _renaming_path.parent_path() / new_name;

  if (new_relative == _renaming_path) {
    return;
  }

  const auto old_absolute = project.assets_directory() / _renaming_path;
  const auto new_absolute = project.assets_directory() / new_relative;

  if (std::filesystem::exists(new_absolute)) {
    return; // name clash: discarded like a click-away cancel
  }

  if (!assets_module.move_asset(old_absolute, new_absolute)) {
    return;
  }

  _needs_refresh = true;

  if (const auto* selected = std::get_if<asset_selection>(&state.current_selection); selected != nullptr && selected->path == _renaming_path) {
    state.select_asset(selected->id, new_relative, selected->kind);
  }
}

auto asset_browser_panel::_draw_move_drop_target(editor_state& state, const std::filesystem::path& destination_directory_relative) -> void {
  if (ImGui::BeginDragDropTarget()) {
    if (const auto* payload = ImGui::AcceptDragDropPayload(asset_move_drag_payload_type)) {
      _try_move(state, path_from_move_payload(*payload), destination_directory_relative);
    }

    ImGui::EndDragDropTarget();
  }
}

auto asset_browser_panel::_draw_entry_context_menu(editor_state& state, const asset_browser_entry& entry) -> void {
  const auto target_directory = entry.is_directory ? entry.path : entry.path.parent_path();

  // A menu alternative to dragging a prefab into the Hierarchy.
  if (!entry.is_directory && entry.kind == asset_kind::prefab && ImGui::MenuItem(ICON_MDI_CUBE_SCAN " Instantiate in Scene")) {
    auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

    if (auto prefab = assets_module.load_prefab(entry.id); prefab.is_valid()) {
      auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
      auto& scene = scenes_module.active_scene();

      auto command = std::make_unique<instantiate_prefab_command>(prefab);
      auto* created = command.get();

      state.push_command(scene, std::move(command));
      state.select_node(scene.find(created->id()));
    }
  }

  if (ImGui::BeginMenu(ICON_MDI_FOLDER_PLUS " Create")) {
    _draw_create_menu(state, target_directory);
    ImGui::EndMenu();
  }

  ImGui::Separator();

  if (ImGui::MenuItem(ICON_MDI_PENCIL " Rename")) {
    _begin_rename(entry.path, entry.is_directory);
  }

  if (ImGui::MenuItem(ICON_MDI_CONTENT_DUPLICATE " Duplicate")) {
    _duplicate(state, entry.path, entry.is_directory);
  }

  if (ImGui::MenuItem(ICON_MDI_DELETE " Delete")) {
    _request_delete(entry.path, entry.is_directory);
  }
}

auto asset_browser_panel::draw(editor_state& state) -> void {
  // The panes scroll on their own; the panel never needs to.
  ImGui::Begin(window_name, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

  auto& project = sbx::core::engine::project();

  // One-shot reveal requests, so the user can navigate freely afterwards.
  if (state.reveal_in_browser_request) {
    _navigate_to(state.reveal_in_browser_request->parent_path());
    state.reveal_in_browser_request.reset();
  }

  if (auto picked = _import_dialog.result()) {
    _pending_asset_imports.insert(_pending_asset_imports.end(), picked->begin(), picked->end());
  }

  _process_pending_asset_imports(state);

  if (ImGui::Button(ICON_MDI_FOLDER_PLUS " Create")) {
    ImGui::OpenPopup("##asset_browser_create_menu");
  }

  if (ImGui::BeginPopup("##asset_browser_create_menu")) {
    _draw_create_menu(state, _current_directory);
    ImGui::EndPopup();
  }

  ImGui::Separator();

  // Breadcrumbs: the root, then one button per segment, each also a move drop target. Iterates a copy, since clicking reassigns _current_directory mid-loop.
  if (ImGui::Button(ICON_MDI_FOLDER_OPEN " assets")) {
    _navigate_to(std::filesystem::path{});
  }

  _draw_move_drop_target(state, std::filesystem::path{});

  const auto breadcrumb_directory = _current_directory;
  auto breadcrumb_path = std::filesystem::path{};

  for (const auto& segment : breadcrumb_directory) {
    breadcrumb_path /= segment;

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextDisabled("%s", ICON_MDI_CHEVRON_RIGHT);
    ImGui::SameLine(0.0f, 4.0f);

    ImGui::PushID(breadcrumb_path.string().c_str());

    if (ImGui::Button(segment.string().c_str())) {
      _navigate_to(breadcrumb_path);
    }

    _draw_move_drop_target(state, breadcrumb_path);

    ImGui::PopID();
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(160.0f);
  ImGui::InputTextWithHint("##asset_browser_search", ICON_MDI_MAGNIFY " Search...", _search_filter.data(), _search_filter.size());

  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0f);
  ImGui::SliderFloat("##asset_browser_tile_size", &_tile_size, 48.0f, 128.0f, "%.0f px");

  if (_needs_refresh) {
    _refresh_entries();
  }

  // Both panes get the remaining height minus the table's cell padding, so the panel never overflows.
  const auto panes_height = ImGui::GetContentRegionAvail().y - ImGui::GetStyle().CellPadding.y * 2.0f;

  // A narrow, resizable folder tree column; the contents take the rest.
  if (ImGui::BeginTable("asset_browser_columns", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Contents", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::BeginChild("##asset_browser_tree_scroll", ImVec2(0.0f, panes_height));

    const auto root_is_leaf = !has_visible_subdirectories(project.assets_directory());

    auto root_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (_current_directory.empty()) {
      root_flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (root_is_leaf) {
      root_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    if (_pending_reveal) {
      ImGui::SetNextItemOpen(true);
    }

    const auto is_root_open = ImGui::TreeNodeEx("##assets_root", root_flags, "%s assets", ICON_MDI_FOLDER_OPEN);

    if (_pending_reveal && _pending_reveal->empty()) {
      ImGui::SetScrollHereY(0.5f);
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      _navigate_to(std::filesystem::path{});
    }

    _draw_move_drop_target(state, std::filesystem::path{});

    if (ImGui::BeginPopupContextItem("##assets_root_context")) {
      if (ImGui::BeginMenu(ICON_MDI_FOLDER_PLUS " Create")) {
        _draw_create_menu(state, std::filesystem::path{});
        ImGui::EndMenu();
      }

      ImGui::EndPopup();
    }

    if (is_root_open && !root_is_leaf) {
      _draw_directory_tree(state, project.assets_directory(), std::filesystem::path{});
      ImGui::TreePop();
    }

    // Only forced open for one frame, so the tree can be collapsed by hand afterwards.
    _pending_reveal.reset();

    ImGui::EndChild();

    ImGui::TableSetColumnIndex(1);
    ImGui::BeginChild("##asset_browser_contents_scroll", ImVec2(0.0f, panes_height));

    _draw_asset_grid(state);

    ImGui::EndChild();

    ImGui::EndTable();
  }

  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2, false)) {
    if (const auto* selected = std::get_if<asset_selection>(&state.current_selection); selected != nullptr) {
      _begin_rename(selected->path, false);
    }
  }

  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
    if (const auto* selected = std::get_if<asset_selection>(&state.current_selection); selected != nullptr) {
      _request_delete(selected->path, false);
    }
  }

  _draw_import_and_delete_dialogs(state);

  ImGui::End();
}

} // namespace editor
