// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_ASSET_METADATA_HPP_
#define EDITOR_PANELS_ASSET_METADATA_HPP_

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include <libsbx/math/uuid.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/** @brief One row cached by the Asset Browser for the currently browsed directory. */
struct asset_browser_entry {
  std::filesystem::path path{}; // project-relative
  bool is_directory{false};
  asset_kind kind{asset_kind::unknown};
  bool is_importable{false};
  sbx::math::uuid id{sbx::math::uuid::nil()}; // resolved lazily, on click
}; // struct asset_browser_entry

/** @brief Single source of truth for both classify_extension and importable_extensions, so the "Import from Disk..." file dialog's filter can never drift from what a dropped-in file would actually be classified as. */
auto extension_table() -> const std::unordered_map<std::string, asset_kind>&;

[[nodiscard]] auto is_importable_kind(asset_kind kind) -> bool;

auto classify_extension(const std::filesystem::path& extension) -> asset_kind;

/** @brief Every extension "Import from Disk..."'s file dialog should offer — everything classify_extension routes through assets_module::import (i.e. every importable kind; .yaml/scene is excluded, same as the per-entry Import path). */
auto importable_extensions() -> std::vector<std::string>;

auto icon_for(const asset_browser_entry& entry) -> const char*;

auto is_entry_selected(const editor_state& state, const std::filesystem::path& path) -> bool;

/** @brief Which drag_drop_payload_* (asset_tile.hpp) a tile of this kind carries -- nullptr for kinds no Inspector picker ever accepts (environment_map, scene, script), which just aren't draggable into a picker slot (they can still carry the move payload below). */
auto drag_payload_type_for(asset_kind kind) -> const char*;

/**
 * @brief Browser-local "move this into a folder" drag payload -- distinct from the kind-specific
 * drag_drop_payload_* above (which Inspector pickers accept), this one only ever moves between
 * tiles/tree nodes of this same panel, and (unlike the kind-specific payload) both files and
 * folders offer it.
 */
inline constexpr auto asset_move_drag_payload_type = "SBX_ASSET_BROWSER_MOVE";

struct asset_move_drag_payload {
  char path[256]{}; // project-relative, null-terminated
}; // struct asset_move_drag_payload

auto make_move_drag_payload(const std::filesystem::path& relative_path) -> asset_move_drag_payload;
auto path_from_move_payload(const ImGuiPayload& payload) -> std::filesystem::path;

/** @brief Case-insensitive alphabetical order, shared by the folder tree and the contents pane so both panes read as "sorted" the same way. */
[[nodiscard]] auto filename_less(const std::filesystem::path& lhs, const std::filesystem::path& rhs) -> bool;

/** @brief Truncates (with an ellipsis) rather than wrapping, so a long filename never grows a grid row taller than the fixed height ImGuiListClipper assumes, and never overflows sideways into the next tile's column. */
auto truncate_to_width(const std::string& text, std::float_t max_width) -> std::string;

/**
 * @brief Internal/tooling entries the Asset Browser should never surface, regardless of the search
 * filter: .meta sidecars (asset_cooker's per-source uuid, not content of their own), the IDE-only
 * Game.csproj the engine regenerates every start purely for IntelliSense, its bin/obj build
 * output, and any dotfile/hidden entry (.git, .vs, .vscode, ...).
 */
auto is_hidden_from_browser(const std::filesystem::path& name, bool is_directory) -> bool;

/** @brief Whether `directory` contains at least one subdirectory the tree would actually show -- used to suppress the expand arrow on leaf directories, since there's nothing for it to expand into. */
[[nodiscard]] auto has_visible_subdirectories(const std::filesystem::path& directory) -> bool;

/**
 * @brief True if `candidate` is `target` itself or one of its ancestors (path-component prefix) --
 * used to decide which nodes along a "reveal" target's chain need to be forced open, and to refuse
 * a drop that would move a folder onto itself or into its own descendant.
 */
[[nodiscard]] auto is_ancestor_or_self(const std::filesystem::path& candidate, const std::filesystem::path& target) -> bool;

/** @brief Case-insensitive substring test for the search box -- an empty filter matches everything. */
auto filter_matches(std::string_view name, std::string_view filter) -> bool;

/** @brief Finds a filename not already present in @p absolute_directory: "stem.ext", then "stem 1.ext", "stem 2.ext", ... . @p extension may be empty (folders). */
[[nodiscard]] auto unique_name(const std::filesystem::path& absolute_directory, std::string_view stem, std::string_view extension) -> std::string;

} // namespace editor

#endif // EDITOR_PANELS_ASSET_METADATA_HPP_
