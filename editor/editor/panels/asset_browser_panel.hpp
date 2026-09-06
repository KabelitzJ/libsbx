// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_
#define EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_

#include <array>
#include <cmath>
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

#include <libsbx/math/uuid.hpp>

#include <libsbx/render/ui/widgets/file_dialog.hpp>
#include <libsbx/render/ui/fonts/material_design_icons.hpp>

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
 * directory (folder tree left, current folder's contents right).
 *
 * Clicking an importable file registers it with assets_module and selects it in editor_state;
 * clicking a folder navigates into it.
 */
class asset_browser_panel final : public editor_panel {

public:

  /** @see hierarchy_panel::window_name */
  inline static constexpr auto window_name = ICON_MDI_FOLDER_MULTIPLE_IMAGE " Asset Browser###asset_browser_panel";

  auto draw(editor_state& state) -> void override;

private:

  auto _refresh_entries() -> void;
  auto _draw_directory_tree(editor_state& state, const std::filesystem::path& absolute_assets_root, const std::filesystem::path& relative_directory) -> void;

  /** @brief Switches the browsed folder to @p directory (project-relative) and has the tree expand to reveal it -- the single place every navigation (breadcrumb, tree click, grid folder tile) should go through, so the tree never falls out of sync with what's being browsed. */
  auto _navigate_to(std::filesystem::path directory) -> void {
    _current_directory = directory;
    _needs_refresh = true;
    _pending_reveal = std::move(directory);
  }

  /** @brief Drains _import_dialog's result (if any) into _pending_asset_imports, then works through that queue until it's empty or a name clash needs a decision. */
  auto _process_pending_asset_imports(editor_state& state) -> void;

  /** @brief Copies @p source to @p destination (already resolved, clash already handled by the caller) and imports/cooks it — the "Import Asset..." counterpart to the per-entry Import path in draw(). */
  auto _import_asset_file(editor_state& state, const std::filesystem::path& source, const std::filesystem::path& destination) -> void;

  std::filesystem::path _current_directory{}; // project-relative; empty = assets root
  std::vector<asset_browser_entry> _cached_entries{};
  bool _needs_refresh{true};

  // Tile grid state: size is user-adjustable (toolbar slider), search filters _cached_entries by
  // filename substring before layout.
  std::float_t _tile_size{72.0f};
  std::array<char, 128u> _search_filter{};

  // Directory (and its ancestors) the tree should force open for exactly the one frame it's next
  // drawn, set by _navigate_to() -- keeps the tree in sync with whatever folder navigation (a
  // breadcrumb, a tree click, a grid folder tile) just switched _current_directory to.
  std::optional<std::filesystem::path> _pending_reveal{};

  // "Import Mesh" modal state, shown the first time an un-imported .gltf/.glb is clicked.
  // _show_import_mesh_dialog is consumed outside the per-entry PushID scope it's set from, since
  // OpenPopup/BeginPopupModal must see the same ID stack.
  bool _show_import_mesh_dialog{false};
  std::filesystem::path _pending_import_path{};
  bool _import_extract_materials{true};

  // "Import Asset..." pulls a file in from anywhere on disk, unlike the per-entry Import above
  // which only sees files already inside assets/. _pending_asset_imports queues picked absolute
  // paths, processed one at a time so a name clash can pause without losing the rest of a multi-select.
  sbx::render::widgets::file_dialog _import_dialog{};
  std::vector<std::filesystem::path> _pending_asset_imports{};

  bool _import_conflict_unresolved{false}; // true from the clash being found until Overwrite/Skip/Cancel.
  bool _show_import_conflict_dialog{false}; // one-shot OpenPopup trigger, same pattern as _show_import_mesh_dialog.
  std::filesystem::path _import_conflict_source{};      // absolute
  std::filesystem::path _import_conflict_destination{}; // absolute

}; // class asset_browser_panel

} // namespace editor

#endif // EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_
