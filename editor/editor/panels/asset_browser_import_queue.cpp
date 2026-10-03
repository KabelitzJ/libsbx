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

// The indices of checked boxes, or empty when all are checked ("empty means everything").
auto all_checked_indices(const std::vector<bool>& checks) -> std::vector<std::size_t> {
  if (std::ranges::all_of(checks, [](bool checked) { return checked; })) {
    return {};
  }

  auto indices = std::vector<std::size_t>{};

  for (auto index = std::size_t{0u}; index < checks.size(); ++index) {
    if (checks[index]) {
      indices.push_back(index);
    }
  }

  return indices;
}

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
  const auto kind = classify_extension(destination.extension());

  if (kind != asset_kind::mesh || !_defer_mesh_import_if_unseen(relative_path)) {
    const auto id = assets_module.import(destination);
    state.select_asset(id, relative_path, kind);
  }

  _needs_refresh = true;
}

// Queues a mesh without a `.meta` for the import settings dialog and returns true; returns false for known meshes, which the caller imports directly.
auto asset_browser_panel::_defer_mesh_import_if_unseen(const std::filesystem::path& relative_path) -> bool {
  auto& project = sbx::core::engine::project();

  const auto meta_path = std::filesystem::path{project.assets_directory() / relative_path}.concat(".meta");

  if (std::filesystem::exists(meta_path)) {
    return false;
  }

  const auto was_empty = _pending_mesh_imports.empty();
  _pending_mesh_imports.push_back(relative_path);

  if (was_empty) {
    _begin_mesh_import_dialog();
  }

  return true;
}

// Inspects the next queued mesh, resets the options and checkboxes, and shows the dialog. No-op if the queue is empty.
auto asset_browser_panel::_begin_mesh_import_dialog() -> void {
  if (_pending_mesh_imports.empty()) {
    return;
  }

  auto& project = sbx::core::engine::project();

  _mesh_import_summary = sbx::assets::asset_cooker::inspect_mesh_source(project.assets_directory() / _pending_mesh_imports.front());
  _mesh_import_options = sbx::assets::mesh_import_options{};
  _mesh_import_primitive_checks.assign(_mesh_import_summary.has_value() ? _mesh_import_summary->primitives.size() : 0u, true);
  _mesh_import_animation_checks.assign(_mesh_import_summary.has_value() ? _mesh_import_summary->animation_names.size() : 0u, true);
  _show_import_mesh_dialog = true;
}

// The import-mesh, import-conflict and delete-confirmation modals, each opened by its _show_*_dialog flag.
auto asset_browser_panel::_draw_import_and_delete_dialogs(editor_state& state) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (_show_import_mesh_dialog) {
    ImGui::OpenPopup("Import Mesh");
    _show_import_mesh_dialog = false;
  }

  if (ImGui::BeginPopupModal("Import Mesh", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (_pending_mesh_imports.empty()) {
      // Closed with Escape or the window's close button.
      ImGui::CloseCurrentPopup();
    } else {
      const auto& pending_path = _pending_mesh_imports.front();

      ImGui::Text("Import '%s'", pending_path.filename().string().c_str());

      if (_pending_mesh_imports.size() > 1u) {
        ImGui::TextDisabled("%zu more mesh(es) queued after this one.", _pending_mesh_imports.size() - 1u);
      }

      ImGui::Separator();

      if (!_mesh_import_summary.has_value()) {
        ImGui::TextColored(ImVec4{1.0f, 0.6f, 0.2f, 1.0f}, "Could not read this file's contents.");
      } else {
        const auto& summary = *_mesh_import_summary;

        ImGui::TextUnformatted("Primitives");

        for (auto index = std::size_t{0u}; index < summary.primitives.size(); ++index) {
          const auto& primitive = summary.primitives[index];

          ImGui::PushID(static_cast<int>(index));
          auto checked = static_cast<bool>(_mesh_import_primitive_checks[index]);
          ImGui::Checkbox("##primitive_check", &checked);
          _mesh_import_primitive_checks[index] = checked;
          ImGui::SameLine();
          ImGui::Text("%s [%zu]", primitive.mesh_name.c_str(), primitive.primitive_index);
          ImGui::PopID();
        }

        ImGui::Separator();

        ImGui::Checkbox("Extract materials to editable .material assets", &_mesh_import_options.extract_materials);
        ImGui::TextDisabled("Recommended. When off, materials are cooked read-only and won't appear in the Asset Browser.");

        ImGui::BeginDisabled(!summary.has_skeleton);
        ImGui::Checkbox("Import skeleton", &_mesh_import_options.import_skeleton);
        ImGui::EndDisabled();

        if (!summary.has_skeleton) {
          ImGui::TextDisabled("This file has no skeleton.");
        } else if (!summary.animation_names.empty()) {
          ImGui::BeginDisabled(!_mesh_import_options.import_skeleton);
          ImGui::TextUnformatted("Animation clips");

          for (auto index = std::size_t{0u}; index < summary.animation_names.size(); ++index) {
            ImGui::PushID(static_cast<int>(index));
            auto checked = static_cast<bool>(_mesh_import_animation_checks[index]);
            ImGui::Checkbox("##animation_check", &checked);
            _mesh_import_animation_checks[index] = checked;
            ImGui::SameLine();
            ImGui::TextUnformatted(summary.animation_names[index].c_str());
            ImGui::PopID();
          }

          ImGui::EndDisabled();
        }
      }

      ImGui::Separator();

      if (ImGui::Button("Import")) {
        auto options = _mesh_import_options;

        if (_mesh_import_summary.has_value()) {
          options.included_primitives = all_checked_indices(_mesh_import_primitive_checks);
          options.included_animations = all_checked_indices(_mesh_import_animation_checks);
        }

        const auto id = assets_module.import(project.assets_directory() / pending_path);
        assets_module.load_mesh(id, options, /*force_recook=*/true);
        state.select_asset(id, pending_path, asset_kind::mesh);
        _needs_refresh = true; // extracted .material files may now be in this folder

        _pending_mesh_imports.erase(_pending_mesh_imports.begin());
        ImGui::CloseCurrentPopup();
        _begin_mesh_import_dialog();
      }

      ImGui::SameLine();

      if (ImGui::Button("Skip")) {
        _pending_mesh_imports.erase(_pending_mesh_imports.begin());
        ImGui::CloseCurrentPopup();
        _begin_mesh_import_dialog();
      }
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
