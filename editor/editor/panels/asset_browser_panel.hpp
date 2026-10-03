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

#include <libsbx/assets/asset_cooker.hpp>
#include <libsbx/render/ui/widgets/file_dialog.hpp>
#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>
#include <editor/panels/asset_metadata.hpp>

namespace editor {

/** @brief The Asset Browser: a folder tree and the current folder's contents, with create, import, rename, duplicate, delete and drag-to-move. */
class asset_browser_panel final : public editor_panel {

public:

  // The panel's ImGui::Begin() string.
  inline static constexpr auto window_name = ICON_MDI_FOLDER_MULTIPLE_IMAGE " Asset Browser###asset_browser_panel";

  auto draw(editor_state& state) -> void override;

private:

  auto _refresh_entries() -> void;
  auto _draw_directory_tree(editor_state& state, const std::filesystem::path& absolute_assets_root, const std::filesystem::path& relative_directory) -> void;

  /**
   * @brief Browses @p directory and expands the tree to it; every navigation goes through here so the tree stays in sync.
   *
   * @param directory The project-relative folder.
   */
  auto _navigate_to(std::filesystem::path directory) -> void {
    _current_directory = directory;
    _needs_refresh = true;
    _pending_reveal = std::move(directory);
  }

  /** @brief Drains the import dialog into the queue and works through it until done or a name clash needs a decision. */
  auto _process_pending_asset_imports(editor_state& state) -> void;

  /**
   * @brief Copies @p source to the already resolved @p destination and imports it.
   *
   * @param state The editor state.
   * @param source The external file.
   * @param destination The destination inside the assets directory.
   */
  auto _import_asset_file(editor_state& state, const std::filesystem::path& source, const std::filesystem::path& destination) -> void;

  /**
   * @brief The shared Create menu items for @p target_directory, used by the toolbar, empty space and entry context menus.
   *
   * @param state The editor state.
   * @param target_directory The project-relative folder to create in.
   */
  auto _draw_create_menu(editor_state& state, const std::filesystem::path& target_directory) -> void;

  auto _create_material(editor_state& state, const std::filesystem::path& target_directory) -> void;
  auto _create_particle_effect(editor_state& state, const std::filesystem::path& target_directory) -> void;
  auto _create_animation_graph(editor_state& state, const std::filesystem::path& target_directory) -> void;
  auto _create_shader_graph(editor_state& state, const std::filesystem::path& target_directory, bool is_lit) -> void;
  auto _create_scene(editor_state& state, const std::filesystem::path& target_directory) -> void;
  auto _create_script(editor_state& state, const std::filesystem::path& target_directory) -> void;
  auto _create_folder(const std::filesystem::path& target_directory) -> void;

  auto _begin_rename(const std::filesystem::path& relative_path, bool is_directory) -> void;

  /**
   * @brief The in-place rename field, shared by the grid and the tree.
   *
   * @param state The editor state.
   * @param width The field width.
   */
  auto _draw_rename_field(editor_state& state, std::float_t width) -> void;

  auto _commit_rename(editor_state& state) -> void;

  /**
   * @brief Copies a file or directory under a unique name in the same folder, stripping `.meta` files so the copy gets fresh uuids.
   *
   * @param state The editor state.
   * @param relative_path The entry to copy.
   * @param is_directory Whether the entry is a directory.
   */
  auto _duplicate(editor_state& state, const std::filesystem::path& relative_path, bool is_directory) -> void;

  /**
   * @brief Opens the delete confirmation; deletion happens once the user confirms.
   *
   * @param relative_path The entry to delete.
   * @param is_directory Whether the entry is a directory.
   */
  auto _request_delete(const std::filesystem::path& relative_path, bool is_directory) -> void;

  /**
   * @brief Moves an entry into a folder, refusing no-op moves, name clashes and moving a folder into its own subtree.
   *
   * @param state The editor state.
   * @param source_relative The entry to move.
   * @param destination_directory_relative The destination folder.
   */
  auto _try_move(editor_state& state, const std::filesystem::path& source_relative, const std::filesystem::path& destination_directory_relative) -> void;

  /** @brief The search-filtered, row-clipped tile grid, drawn into the caller's child window. */
  auto _draw_asset_grid(editor_state& state) -> void;

  /**
   * @brief Makes the previous widget a move drop target; call right after it.
   *
   * @param state The editor state.
   * @param destination_directory_relative The folder dropped entries move into.
   */
  auto _draw_move_drop_target(editor_state& state, const std::filesystem::path& destination_directory_relative) -> void;

  /**
   * @brief The entry context menu (Create/Rename/Duplicate/Delete, plus Instantiate for prefabs). Call inside an open BeginPopupContextItem().
   *
   * @param state The editor state.
   * @param entry The entry.
   */
  auto _draw_entry_context_menu(editor_state& state, const asset_browser_entry& entry) -> void;

  /** @brief The import-mesh, import-conflict and delete-confirmation modals. */
  auto _draw_import_and_delete_dialogs(editor_state& state) -> void;

  /**
   * @brief Queues a mesh without a `.meta` for the import settings dialog.
   *
   * @param relative_path The mesh file.
   *
   * @return True if queued; false for known meshes, which the caller imports directly.
   */
  auto _defer_mesh_import_if_unseen(const std::filesystem::path& relative_path) -> bool;

  /** @brief Inspects the next queued mesh, resets the options and checkboxes, and shows the dialog. No-op if the queue is empty. */
  auto _begin_mesh_import_dialog() -> void;

  std::filesystem::path _current_directory{};
  std::vector<asset_browser_entry> _cached_entries{};
  bool _needs_refresh{true};

  std::float_t _tile_size{72.0f};
  std::array<char, 128u> _search_filter{};

  std::optional<std::filesystem::path> _pending_reveal{};

  std::vector<std::filesystem::path> _pending_mesh_imports{};
  bool _show_import_mesh_dialog{false};
  std::optional<sbx::assets::mesh_source_summary> _mesh_import_summary{};
  sbx::assets::mesh_import_options _mesh_import_options{};
  std::vector<bool> _mesh_import_primitive_checks{};
  std::vector<bool> _mesh_import_animation_checks{};

  sbx::render::file_dialog _import_dialog{};
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
