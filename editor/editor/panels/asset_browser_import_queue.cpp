// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/asset_browser_panel.hpp>

#include <algorithm>
#include <filesystem>
#include <system_error>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace editor {

// Drains the import dialog's result into the queue, then works through it until done or a name clash needs a decision.
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

// Copies @p source to the already resolved @p destination and imports it.
auto asset_browser_panel::_import_asset_file(editor_state& state, const std::filesystem::path& source, const std::filesystem::path& destination) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  std::filesystem::create_directories(destination.parent_path());

  // copy_file throws when source and destination are the same file; then just re-import.
  auto ec = std::error_code{};
  if (!std::filesystem::equivalent(source, destination, ec)) {
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);

    // A loose .gltf needs its referenced .bin and images next to it, or inspecting and cooking fail.
    if (destination.extension() == ".gltf") {
      for (const auto& reference : sbx::assets::asset_cooker::gltf_external_file_references(source)) {
        const auto reference_source = source.parent_path() / reference;
        const auto reference_destination = destination.parent_path() / reference;

        if (std::filesystem::exists(reference_source)) {
          std::filesystem::create_directories(reference_destination.parent_path());
          std::filesystem::copy_file(reference_source, reference_destination, std::filesystem::copy_options::overwrite_existing);
        }
      }
    }
  }

  const auto relative_path = std::filesystem::relative(destination, project.assets_directory());
  const auto kind = sbx::assets::asset_type_of(destination);

  if (!_defer_import_if_unseen(state, relative_path)) {
    const auto id = assets_module.import(destination);
    state.select_asset(id, relative_path, kind);
  }

  _needs_refresh = true;
}

// Queues a file whose type has an importer but no `.meta` yet for the import window and returns true; false means the caller imports it directly.
auto asset_browser_panel::_defer_import_if_unseen(editor_state& state, const std::filesystem::path& relative_path) -> bool {
  auto& project = sbx::core::engine::project();

  if (!sbx::assets::has_import_settings(sbx::assets::asset_type_of(relative_path))) {
    return false;
  }

  const auto meta_path = std::filesystem::path{project.assets_directory() / relative_path}.concat(".meta");

  if (std::filesystem::exists(meta_path)) {
    return false;
  }

  state.request_import(relative_path);

  return true;
}

// The import-conflict and delete-confirmation modals, each opened by its _show_*_dialog flag.
auto asset_browser_panel::_draw_import_and_delete_dialogs(editor_state& state) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

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
