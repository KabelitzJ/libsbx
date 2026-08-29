// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/asset_browser_panel.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/material.hpp>
#include <libsbx/assets/particle_effect.hpp>

namespace editor {

namespace {

// Single source of truth for both classify_extension (below) and importable_extensions, so the
// "Import Asset..." file dialog's filter (see asset_browser_panel::draw) can never drift from
// what a dropped-in file would actually be classified as.
auto extension_table() -> const std::unordered_map<std::string, asset_kind>& {
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
    {".cs", asset_kind::script},
  };

  return table;
}

[[nodiscard]] auto is_importable_kind(asset_kind kind) -> bool {
  return kind == asset_kind::texture || kind == asset_kind::mesh ||
         kind == asset_kind::material || kind == asset_kind::environment_map ||
         kind == asset_kind::particle_effect;
}

} // namespace

auto classify_extension(const std::filesystem::path& extension) -> asset_kind {
  const auto& table = extension_table();
  const auto entry = table.find(extension.string());

  return entry != table.end() ? entry->second : asset_kind::unknown;
}

/** @brief Every extension "Import Asset..."'s file dialog should offer — everything classify_extension routes through assets_module::import (i.e. every importable kind; .yaml/scene is excluded, same as the per-entry Import path). */
auto importable_extensions() -> std::vector<std::string> {
  auto extensions = std::vector<std::string>{};

  for (const auto& [extension, kind] : extension_table()) {
    if (is_importable_kind(kind)) {
      extensions.push_back(extension);
    }
  }

  return extensions;
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
    case asset_kind::script: return ICON_MDI_FILE_CODE_OUTLINE;
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
      entry.is_importable = is_importable_kind(entry.kind);
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
  ImGui::Begin(window_name);

  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (auto picked = _import_dialog.result()) {
    _pending_asset_imports.insert(_pending_asset_imports.end(), picked->begin(), picked->end());
  }

  _process_pending_asset_imports(state);

  if (ImGui::Button(ICON_MDI_FILE_IMPORT " Import Asset...")) {
    _import_dialog.open("Import Asset", sbx::render::widgets::file_dialog_mode::open_files, project.assets_directory() / _current_directory, importable_extensions());
  }

  ImGui::SameLine();

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

  if (ImGui::Button(ICON_MDI_FILE_CODE_OUTLINE " New Script")) {
    auto file_name = std::string{"NewScript.cs"};
    auto suffix = 1;

    while (std::filesystem::exists(project.assets_directory() / _current_directory / file_name)) {
      file_name = fmt::format("NewScript{}.cs", suffix++);
    }

    const auto relative_path = _current_directory / file_name;
    const auto absolute_path = project.assets_directory() / relative_path;
    const auto class_name = std::filesystem::path{file_name}.stem().string();

    std::filesystem::create_directories(absolute_path.parent_path());

    auto out = std::ofstream{absolute_path};
    out << fmt::format(
      "using Sbx.Core;\n\n"
      "public class {} : Behavior\n"
      "{{\n"
      "    public override void OnCreate()\n"
      "    {{\n"
      "    }}\n\n"
      "    public override void OnUpdate()\n"
      "    {{\n"
      "    }}\n\n"
      "    public override void OnDestroy()\n"
      "    {{\n"
      "    }}\n"
      "}}\n",
      class_name
    );

    _needs_refresh = true;
    state.select_asset(sbx::math::uuid::nil(), relative_path, asset_kind::script);
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
      } else if (entry.kind == asset_kind::script) {
        if (ImGui::Selectable(label.c_str(), is_entry_selected(state, entry.path))) {
          state.select_asset(sbx::math::uuid::nil(), entry.path, asset_kind::script);
        }
      } else {
        ImGui::TextDisabled("%s", label.c_str());
      }

      ImGui::PopID();
    }

    // Right-click the empty area of the contents pane (not an entry — see NoOpenOverItems) for
    // the same "Import Asset..." action as the toolbar button above.
    if (ImGui::BeginPopupContextWindow("##asset_browser_context", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
      if (ImGui::MenuItem(ICON_MDI_FILE_IMPORT " Import Asset...")) {
        _import_dialog.open("Import Asset", sbx::render::widgets::file_dialog_mode::open_files, project.assets_directory() / _current_directory, importable_extensions());
      }

      ImGui::EndPopup();
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

  _import_dialog.draw();

  if (_show_import_conflict_dialog) {
    ImGui::OpenPopup("Import Conflict");
    _show_import_conflict_dialog = false;
  }

  if (ImGui::BeginPopupModal("Import Conflict", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("'%s' already exists in this folder.", _import_conflict_destination.filename().string().c_str());

    if (ImGui::Button("Overwrite")) {
      _import_asset_file(state, _import_conflict_source, _import_conflict_destination);
      _import_conflict_unresolved = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Skip")) {
      _import_conflict_unresolved = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel Remaining")) {
      _pending_asset_imports.clear();
      _import_conflict_unresolved = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  ImGui::End();
}

auto asset_browser_panel::_process_pending_asset_imports(editor_state& state) -> void {
  auto& project = sbx::core::engine::project();

  while (!_pending_asset_imports.empty() && !_import_conflict_unresolved) {
    const auto source = _pending_asset_imports.front();
    _pending_asset_imports.erase(_pending_asset_imports.begin());

    const auto destination = project.assets_directory() / _current_directory / source.filename();

    if (std::filesystem::exists(destination)) {
      _import_conflict_source = source;
      _import_conflict_destination = destination;
      _import_conflict_unresolved = true;
      _show_import_conflict_dialog = true;

      break;
    }

    _import_asset_file(state, source, destination);
  }
}

auto asset_browser_panel::_import_asset_file(editor_state& state, const std::filesystem::path& source, const std::filesystem::path& destination) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  std::filesystem::create_directories(destination.parent_path());

  // copy_file throws if source and destination are the same file (e.g. picking a file already
  // inside the current folder via the dialog) — nothing to copy in that case, just (re-)import it.
  auto ec = std::error_code{};
  if (!std::filesystem::equivalent(source, destination, ec)) {
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
  }

  const auto relative_path = std::filesystem::relative(destination, project.assets_directory());
  const auto kind = classify_extension(destination.extension());

  if (kind == asset_kind::mesh && !std::filesystem::exists(std::filesystem::path{destination}.concat(".meta"))) {
    // First time this mesh has ever been seen — same "let the user choose extract_materials"
    // detour the per-entry Import path takes, and the same modal handles both (see draw()).
    _pending_import_path = relative_path;
    _import_extract_materials = true;
    _show_import_mesh_dialog = true;
  } else {
    const auto id = assets_module.import(destination);
    state.select_asset(id, relative_path, kind);
  }

  _needs_refresh = true;
}

} // namespace editor
