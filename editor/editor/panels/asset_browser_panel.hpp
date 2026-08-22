// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_
#define EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_

#include <filesystem>
#include <vector>

#include <libsbx/math/uuid.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/** @brief One row cached by the Asset Browser for the currently browsed directory. */
struct asset_browser_entry {
  std::filesystem::path path{}; // project-relative
  bool is_directory{false};
  asset_kind kind{asset_kind::unknown};
  bool is_importable{false};
  sbx::math::uuid id{sbx::math::uuid::nil()}; // resolved lazily, on click
}; // struct asset_browser_entry

/**
 * @brief Draws the "Asset Browser" panel: a two-pane view of the active project's assets
 * directory (folder tree left, current folder's contents right). Clicking an importable file
 * registers it with assets_module and selects it in the shared editor_state; clicking a folder
 * navigates into it. Which directory is browsed and its cached listing are this panel's own
 * state — no other panel needs them.
 */
class asset_browser_panel final : public editor_panel {

public:

  auto draw(editor_state& state) -> void override;

private:

  auto _refresh_entries() -> void;
  auto _draw_directory_tree(editor_state& state, const std::filesystem::path& absolute_assets_root, const std::filesystem::path& relative_directory) -> void;

  std::filesystem::path _current_directory{}; // project-relative; empty = assets root
  std::vector<asset_browser_entry> _cached_entries{};
  bool _needs_refresh{true};

  // "Import Mesh" modal state — shown once, the first time a .gltf/.glb without a .meta yet is
  // clicked, to surface mesh_import_options::extract_materials before the mesh is actually cooked.
  // _show_import_mesh_dialog is consumed (and ImGui::OpenPopup called) outside the per-entry
  // PushID scope it's set from — OpenPopup/BeginPopupModal must see the same ID stack, and the
  // click happens inside PushID(entry.path...).
  bool _show_import_mesh_dialog{false};
  std::filesystem::path _pending_import_path{};
  bool _import_extract_materials{true};

}; // class asset_browser_panel

} // namespace editor

#endif // EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_
