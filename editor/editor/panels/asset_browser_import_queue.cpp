// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/asset_browser_panel.hpp>

#include <filesystem>
#include <system_error>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace editor {

// Drains _import_dialog's result (if any) into _pending_asset_imports, then works through that
// queue until it's empty or a name clash needs a decision.
auto asset_browser_panel::_process_pending_asset_imports(editor_state& state) -> void {
  auto& project = sbx::core::engine::project();

  while (!_pending_asset_imports.empty() && !_import_conflict_unresolved) {
    const auto source = _pending_asset_imports.front();
    _pending_asset_imports.erase(_pending_asset_imports.begin());

    const auto destination = project.assets_directory() / _import_destination_directory / source.filename();

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

// Copies @p source to @p destination (already resolved, clash already handled by the caller) and
// imports/cooks it — the "Import from Disk..." counterpart to the per-entry Import path in
// asset_browser_grid_view.cpp.
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
    // detour the per-entry Import path takes, and the same modal handles both (see
    // _draw_import_and_delete_dialogs).
    _pending_import_path = relative_path;
    _import_extract_materials = true;
    _show_import_mesh_dialog = true;
  } else {
    const auto id = assets_module.import(destination);
    state.select_asset(id, relative_path, kind);
  }

  _needs_refresh = true;
}

// The import-mesh, import-conflict, and delete-confirmation modals -- each opens itself from its
// own _show_*_dialog flag, set elsewhere (the per-entry click path, _process_pending_asset_imports,
// and _request_delete respectively).
auto asset_browser_panel::_draw_import_and_delete_dialogs(editor_state& state) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

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

  if (_show_delete_confirm_dialog) {
    ImGui::OpenPopup("Delete Asset");
    _show_delete_confirm_dialog = false;
  }

  if (ImGui::BeginPopupModal("Delete Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (_pending_delete_is_directory) {
      ImGui::Text("Delete folder '%s' and everything inside it?", _pending_delete_path.filename().string().c_str());
    } else {
      ImGui::Text("Delete '%s'?", _pending_delete_path.filename().string().c_str());
    }

    ImGui::TextDisabled("This cannot be undone.");

    if (ImGui::Button(ICON_MDI_DELETE " Delete")) {
      assets_module.delete_asset(project.assets_directory() / _pending_delete_path);

      if (const auto* selected = std::get_if<asset_selection>(&state.current_selection); selected != nullptr && is_ancestor_or_self(_pending_delete_path, selected->path)) {
        state.clear_selection();
      }

      _needs_refresh = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

} // namespace editor
