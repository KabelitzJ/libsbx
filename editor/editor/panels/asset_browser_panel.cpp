// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/asset_browser_panel.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>

#include <fmt/format.h>

#include <imgui.h>

#include <editor/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/particle_effect.hpp>

namespace editor {

auto classify_extension(const std::filesystem::path& extension) -> asset_kind {
  static const auto table = std::unordered_map<std::string, asset_kind>{
    {".png", asset_kind::texture},
    {".jpg", asset_kind::texture},
    {".jpeg", asset_kind::texture},
    {".gltf", asset_kind::mesh},
    {".glb", asset_kind::mesh},
    {".material", asset_kind::material},
    {".hdr", asset_kind::environment_map},
    {".particle_effect", asset_kind::particle_effect},
    {".yaml", asset_kind::scene},
  };

  const auto entry = table.find(extension.string());

  return entry != table.end() ? entry->second : asset_kind::unknown;
}

auto icon_for(const asset_browser_entry& entry) -> const char* {
  if (entry.is_directory) {
    return ICON_MDI_FOLDER;
  }

  switch (entry.kind) {
    case asset_kind::texture: return ICON_MDI_IMAGE;
    case asset_kind::mesh: return ICON_MDI_CUBE_OUTLINE;
    case asset_kind::material: return ICON_MDI_PALETTE_SWATCH;
    case asset_kind::environment_map: return ICON_MDI_EARTH;
    case asset_kind::particle_effect: return ICON_MDI_FIREWORK;
    case asset_kind::scene: return ICON_MDI_FILE_TREE;
    case asset_kind::unknown: return ICON_MDI_FILE_OUTLINE;
  }

  return ICON_MDI_FILE_OUTLINE;
}

auto is_entry_selected(const editor_state& state, const std::filesystem::path& path) -> bool {
  const auto* selected = std::get_if<asset_selection>(&state.current_selection);
  return selected != nullptr && selected->path == path;
}

auto asset_browser_panel::_refresh_entries() -> void {
  _cached_entries.clear();

  auto& project = sbx::core::engine::project();
  const auto absolute_directory = project.assets_directory() / _current_directory;

  if (!std::filesystem::exists(absolute_directory)) {
    _needs_refresh = false;
    return;
  }

  for (const auto& dir_entry : std::filesystem::directory_iterator{absolute_directory}) {
    auto entry = asset_browser_entry{};
    entry.path = _current_directory / dir_entry.path().filename();
    entry.is_directory = dir_entry.is_directory();

    if (!entry.is_directory) {
      entry.kind = classify_extension(dir_entry.path().extension());
      entry.is_importable = entry.kind == asset_kind::texture || entry.kind == asset_kind::mesh ||
                             entry.kind == asset_kind::material || entry.kind == asset_kind::environment_map ||
                             entry.kind == asset_kind::particle_effect;
    }

    _cached_entries.push_back(std::move(entry));
  }

  std::ranges::sort(_cached_entries, [](const auto& lhs, const auto& rhs) {
    if (lhs.is_directory != rhs.is_directory) {
      return lhs.is_directory > rhs.is_directory;
    }

    return lhs.path.filename() < rhs.path.filename();
  });

  _needs_refresh = false;
}

// Recursively lists subdirectories only, live per expanded node — cheap (names only, no imports).
auto asset_browser_panel::_draw_directory_tree(editor_state& state, const std::filesystem::path& absolute_assets_root, const std::filesystem::path& relative_directory) -> void {
  const auto absolute_directory = absolute_assets_root / relative_directory;

  if (!std::filesystem::exists(absolute_directory)) {
    return;
  }

  for (const auto& dir_entry : std::filesystem::directory_iterator{absolute_directory}) {
    if (!dir_entry.is_directory()) {
      continue;
    }

    const auto relative_child = relative_directory / dir_entry.path().filename();
    const auto child_name = dir_entry.path().filename().string();

    auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (_current_directory == relative_child) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(relative_child.string().c_str());

    const auto is_open = ImGui::TreeNodeEx("##dir", flags, "%s %s", ICON_MDI_FOLDER, child_name.c_str());

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      _current_directory = relative_child;
      _needs_refresh = true;
    }

    if (is_open) {
      _draw_directory_tree(state, absolute_assets_root, relative_child);
      ImGui::TreePop();
    }

    ImGui::PopID();
  }
}

auto asset_browser_panel::draw(editor_state& state) -> void {
  ImGui::Begin(ICON_MDI_FOLDER_MULTIPLE_IMAGE " Asset Browser###asset_browser_panel");

  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (ImGui::Button("Import All in This Folder")) {
    // import_directory (like import()) needs a path resolvable from cwd, not one merely
    // relative to assets_directory() — see assets_module.hpp's doc comment.
    assets_module.import_directory(project.assets_directory() / _current_directory);
    _needs_refresh = true;
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_MDI_PLUS " New Material")) {
    auto file_name = std::string{"New Material.material"};
    auto suffix = 1;

    while (std::filesystem::exists(project.assets_directory() / _current_directory / file_name)) {
      file_name = fmt::format("New Material {}.material", suffix++);
    }

    const auto relative_path = _current_directory / file_name;

    auto handle = assets_module.create_material(sbx::assets::material::create_info{.name = "New Material"});
    const auto id = assets_module.save_material(handle, relative_path);

    _needs_refresh = true;
    state.select_asset(id, relative_path, asset_kind::material);
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_MDI_FIREWORK " New Particle Effect")) {
    auto file_name = std::string{"New Particle Effect.particle_effect"};
    auto suffix = 1;

    while (std::filesystem::exists(project.assets_directory() / _current_directory / file_name)) {
      file_name = fmt::format("New Particle Effect {}.particle_effect", suffix++);
    }

    const auto relative_path = _current_directory / file_name;

    auto handle = assets_module.create_particle_effect(sbx::assets::particle_effect::create_info{.name = "New Particle Effect"});
    const auto id = assets_module.save_particle_effect(handle, relative_path);

    _needs_refresh = true;
    state.select_asset(id, relative_path, asset_kind::particle_effect);
  }

  ImGui::SameLine();
  ImGui::TextDisabled("assets/%s", _current_directory.string().c_str());

  if (_needs_refresh) {
    _refresh_entries();
  }

  // Folder tree gets a narrow fixed-width column (user-resizable); contents gets the rest.
  if (ImGui::BeginTable("asset_browser_columns", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableSetupColumn("Contents", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);

    auto root_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (_current_directory.empty()) {
      root_flags |= ImGuiTreeNodeFlags_Selected;
    }

    const auto is_root_open = ImGui::TreeNodeEx("##assets_root", root_flags, "%s assets", ICON_MDI_FOLDER_OPEN);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      _current_directory.clear();
      _needs_refresh = true;
    }

    if (is_root_open) {
      _draw_directory_tree(state, project.assets_directory(), std::filesystem::path{});
      ImGui::TreePop();
    }

    ImGui::TableSetColumnIndex(1);

    // Current directory's immediate contents.
    if (!_current_directory.empty()) {
      if (ImGui::Selectable(ICON_MDI_ARROW_LEFT " ..")) {
        _current_directory = _current_directory.parent_path();
        _needs_refresh = true;
      }
    }

    if (_cached_entries.empty()) {
      ImGui::TextDisabled("Nothing here.");
    }

    for (auto& entry : _cached_entries) {
      ImGui::PushID(entry.path.string().c_str());

      const auto label = std::string{icon_for(entry)} + " " + entry.path.filename().string();

      if (entry.is_directory) {
        if (ImGui::Selectable(label.c_str())) {
          _current_directory = entry.path;
          _needs_refresh = true;
        }
      } else if (entry.is_importable) {
        if (ImGui::Selectable(label.c_str(), is_entry_selected(state, entry.path))) {
          const auto meta_path = std::filesystem::path{project.assets_directory() / entry.path}.concat(".meta");

          if (entry.kind == asset_kind::mesh && !std::filesystem::exists(meta_path)) {
            // First time this mesh has ever been seen — let the user decide whether to extract
            // its materials before it's actually cooked (deferring would mean the choice has
            // nowhere to be remembered until something else needs the mesh).
            _pending_import_path = entry.path;
            _import_extract_materials = true;
            _show_import_mesh_dialog = true;
          } else {
            // Same resolution requirement as above — entry.path is relative to assets_directory().
            entry.id = assets_module.import(project.assets_directory() / entry.path);
            state.select_asset(entry.id, entry.path, entry.kind);
          }
        }
      } else if (entry.kind == asset_kind::scene) {
        if (ImGui::Selectable(label.c_str(), is_entry_selected(state, entry.path))) {
          state.select_asset(sbx::math::uuid::nil(), entry.path, asset_kind::scene);
        }
      } else {
        ImGui::TextDisabled("%s", label.c_str());
      }

      ImGui::PopID();
    }

    ImGui::EndTable();
  }

  if (_show_import_mesh_dialog) {
    ImGui::OpenPopup("Import Mesh");
    _show_import_mesh_dialog = false;
  }

  if (ImGui::BeginPopupModal("Import Mesh", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Import '%s'", _pending_import_path.filename().string().c_str());
    ImGui::Checkbox("Extract materials to editable .material assets", &_import_extract_materials);
    ImGui::TextDisabled("Recommended. When off, materials are cooked read-only and won't appear in the Asset Browser.");

    if (ImGui::Button("Import")) {
      const auto id = assets_module.import(project.assets_directory() / _pending_import_path);
      assets_module.load_mesh(id, sbx::assets::mesh_import_options{.extract_materials = _import_extract_materials});
      state.select_asset(id, _pending_import_path, asset_kind::mesh);
      _needs_refresh = true; // newly extracted .material files may now be visible in this folder
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  ImGui::End();
}

} // namespace editor
