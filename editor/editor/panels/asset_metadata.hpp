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

/** @brief One cached Asset Browser row for the current directory. */
struct asset_browser_entry {
  std::filesystem::path path{}; // project-relative
  bool is_directory{false};
  sbx::assets::asset_type kind{sbx::assets::asset_type::unknown};
  sbx::math::uuid id{sbx::math::uuid::nil()}; // nil until the file has a .meta
}; // struct asset_browser_entry

/**
 * @brief Every extension "Import from Disk..." offers: each type the manifest tracks.
 *
 * @return The extensions.
 */
auto importable_extensions() -> std::vector<std::string>;

auto icon_for(const asset_browser_entry& entry) -> const char*;

auto is_entry_selected(const editor_state& state, const std::filesystem::path& path) -> bool;

// The Asset Browser's own move payload, offered by files and folders and accepted only within the browser.
inline constexpr auto asset_move_drag_payload_type = "SBX_ASSET_BROWSER_MOVE";

struct asset_move_drag_payload {
  char path[256]{}; // project-relative, null-terminated
}; // struct asset_move_drag_payload

auto make_move_drag_payload(const std::filesystem::path& relative_path) -> asset_move_drag_payload;
auto path_from_move_payload(const ImGuiPayload& payload) -> std::filesystem::path;

/**
 * @brief Case-insensitive alphabetical order, shared by the folder tree and the contents pane.
 *
 * @param lhs The first path.
 * @param rhs The second path.
 *
 * @return True if @p lhs sorts first.
 */
[[nodiscard]] auto filename_less(const std::filesystem::path& lhs, const std::filesystem::path& rhs) -> bool;

/**
 * @brief Truncates with an ellipsis instead of wrapping, so grid rows keep the fixed height ImGuiListClipper assumes.
 *
 * @param text The text.
 * @param max_width The available width.
 *
 * @return The fitted text.
 */
auto truncate_to_width(const std::string& text, std::float_t max_width) -> std::string;

/**
 * @brief Entries the Asset Browser never shows: .meta sidecars, the generated Game.csproj and its bin/obj, and dotfiles.
 *
 * @param name The entry's file name.
 * @param is_directory Whether the entry is a directory.
 *
 * @return True if hidden.
 */
auto is_hidden_from_browser(const std::filesystem::path& name, bool is_directory) -> bool;

/**
 * @brief Whether the tree would show any subdirectory of `directory`, to hide the expand arrow on leaves.
 *
 * @param directory The directory.
 *
 * @return True if it has visible subdirectories.
 */
[[nodiscard]] auto has_visible_subdirectories(const std::filesystem::path& directory) -> bool;

/**
 * @brief Whether `candidate` is `target` or one of its ancestors; used for revealing paths and rejecting drops into a folder's own subtree.
 *
 * @param candidate The possible ancestor.
 * @param target The path.
 *
 * @return True if `candidate` is `target` or an ancestor.
 */
[[nodiscard]] auto is_ancestor_or_self(const std::filesystem::path& candidate, const std::filesystem::path& target) -> bool;

/**
 * @brief Case-insensitive substring match; an empty filter matches everything.
 *
 * @param name The text to test.
 * @param filter The search text.
 *
 * @return True on a match.
 */
auto filter_matches(std::string_view name, std::string_view filter) -> bool;

/**
 * @brief A name not yet used in @p absolute_directory: "stem.ext", then "stem 1.ext", "stem 2.ext", and so on.
 *
 * @param absolute_directory The directory.
 * @param stem The base name.
 * @param extension The extension, or empty for folders.
 *
 * @return The unique name.
 */
[[nodiscard]] auto unique_name(const std::filesystem::path& absolute_directory, std::string_view stem, std::string_view extension) -> std::string;

} // namespace editor

#endif // EDITOR_PANELS_ASSET_METADATA_HPP_
