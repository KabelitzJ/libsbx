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
#include <editor/panels/asset_metadata.hpp>

namespace editor {

/**
 * @brief Draws the "Asset Browser" panel: a two-pane, file-explorer-style view of the active
 * project's assets directory (folder tree left, current folder's contents right).
 *
 * Creating assets, importing external files, and making folders all go through a right-click
 * (or the toolbar's "Create" dropdown) rather than dedicated buttons. Entries can be renamed in
 * place (F2 or the context menu), deleted (with confirmation), duplicated, and dragged into a
 * folder to move them; a plain click on an importable file registers it with assets_module and
 * selects it in editor_state, and a folder click navigates into it.
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

  /** @brief Copies @p source to @p destination (already resolved, clash already handled by the caller) and imports/cooks it — the "Import from Disk..." counterpart to the per-entry Import path in draw(). */
  auto _import_asset_file(editor_state& state, const std::filesystem::path& source, const std::filesystem::path& destination) -> void;

  /** @brief Draws the shared "Create" menu items (New Material/Particle Effect/Animation Graph/Script/Folder, Import from Disk..., Reimport All in This Folder) against @p target_directory (project-relative) — shared by the toolbar dropdown, the empty-space context menu, and every per-entry "Create" submenu, so there's exactly one place that knows how to create each asset kind. */
  auto _draw_create_menu(editor_state& state, const std::filesystem::path& target_directory) -> void;

  auto _create_material(editor_state& state, const std::filesystem::path& target_directory) -> void;
  auto _create_particle_effect(editor_state& state, const std::filesystem::path& target_directory) -> void;
  auto _create_animation_graph(editor_state& state, const std::filesystem::path& target_directory) -> void;
  auto _create_script(editor_state& state, const std::filesystem::path& target_directory) -> void;
  auto _create_folder(const std::filesystem::path& target_directory) -> void;

  auto _begin_rename(const std::filesystem::path& relative_path, bool is_directory) -> void;

  /** @brief Draws the in-place rename InputText (replacing a tile's label or a tree row's text) and commits/cancels it -- shared by the grid and the tree, mirroring hierarchy_panel's node rename. */
  auto _draw_rename_field(editor_state& state, std::float_t width) -> void;

  auto _commit_rename(editor_state& state) -> void;

  /** @brief Copies a file or (recursively) a directory under a unique name in the same folder; the copy's `.meta` sidecar(s) are stripped so it mints a fresh uuid on next import rather than sharing identity with the original. */
  auto _duplicate(editor_state& state, const std::filesystem::path& relative_path, bool is_directory) -> void;

  /** @brief Opens the delete-confirmation modal for @p relative_path; the actual assets_module.delete_asset() call happens once the user confirms, in draw(). */
  auto _request_delete(const std::filesystem::path& relative_path, bool is_directory) -> void;

  /** @brief Validates and performs a drag-and-drop (or drop-target) move of @p source_relative into @p destination_directory_relative -- refuses a no-op move, a name clash at the destination, and dropping a folder onto itself or one of its own descendants. */
  auto _try_move(editor_state& state, const std::filesystem::path& source_relative, const std::filesystem::path& destination_directory_relative) -> void;

  /** @brief The tile grid pane: search-filtered, clipped by row, drawn into whatever child window the caller already opened. */
  auto _draw_asset_grid(editor_state& state) -> void;

  /** @brief Wraps the BeginDragDropTarget/AcceptDragDropPayload(asset_move_drag_payload_type)/EndDragDropTarget boilerplate shared by every move-drop target (breadcrumbs, tree nodes, grid folder tiles) -- must be called right after the widget that should act as the target. */
  auto _draw_move_drop_target(editor_state& state, const std::filesystem::path& destination_directory_relative) -> void;

  /** @brief The Create/Rename/Duplicate/Delete context menu shared by a tree node and a grid tile -- an "Instantiate in Scene" item is prepended for a prefab entry. Must be called from inside an already-open ImGui::BeginPopupContextItem() block. */
  auto _draw_entry_context_menu(editor_state& state, const asset_browser_entry& entry) -> void;

  /** @brief The import-mesh, import-conflict, and delete-confirmation modals -- each opens itself from its own _show_*_dialog flag, set elsewhere. */
  auto _draw_import_and_delete_dialogs(editor_state& state) -> void;

  std::filesystem::path _current_directory{};
  std::vector<asset_browser_entry> _cached_entries{};
  bool _needs_refresh{true};

  std::float_t _tile_size{72.0f};
  std::array<char, 128u> _search_filter{};

  std::optional<std::filesystem::path> _pending_reveal{};

  bool _show_import_mesh_dialog{false};
  std::filesystem::path _pending_import_path{};
  bool _import_extract_materials{true};

  sbx::render::widgets::file_dialog _import_dialog{};
  std::filesystem::path _import_destination_directory{};
  std::vector<std::filesystem::path> _pending_asset_imports{};

  bool _import_conflict_unresolved{false};
  bool _show_import_conflict_dialog{false};
  std::filesystem::path _import_conflict_source{};
  std::filesystem::path _import_conflict_destination{};

  std::filesystem::path _renaming_path{};
  bool _renaming_is_directory{false};
  std::array<char, 256u> _rename_buffer{};
  bool _rename_focus_pending{false};

  bool _show_delete_confirm_dialog{false};
  std::filesystem::path _pending_delete_path{};
  bool _pending_delete_is_directory{false};

}; // class asset_browser_panel

} // namespace editor

#endif // EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_
