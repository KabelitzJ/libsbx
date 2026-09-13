// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_UI_WIDGETS_FILE_DIALOG_HPP_
#define LIBSBX_RENDER_UI_WIDGETS_FILE_DIALOG_HPP_

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

namespace sbx::render {

enum class file_dialog_mode : std::uint8_t {
  open_file,     // single file, extension-filtered
  open_files,    // multi-select files, extension-filtered
  select_folder, // pick a directory; no file selection required to confirm
  save_file      // type/pick a filename; confirms through an overwrite prompt if it already exists
}; // enum class file_dialog_mode

/** @brief A named shortcut in the dialog's quick-access sidebar (alongside an always-present "Home"). */
struct file_dialog_shortcut {
  std::string label{};
  std::filesystem::path path{};
}; // struct file_dialog_shortcut

struct file_dialog_options {
  std::string title{};
  file_dialog_mode mode{file_dialog_mode::open_file};

  // Falls back to the user's home directory if empty or not an existing directory.
  std::filesystem::path start_dir{};

  // Filters the listing in every mode but select_folder (which only ever lists directories).
  // Each entry includes its leading dot (e.g. ".gltf"), matched exactly against path::extension().
  // Empty means no filtering. save_file mode also uses extensions.front() to auto-append a missing
  // extension onto whatever the user types.
  std::vector<std::string> extensions{};

  std::vector<file_dialog_shortcut> shortcuts{};

  // save_file mode only -- seeds the filename field.
  std::string default_file_name{};

  // Overrides the confirm button's label (default: "Open"/"Select"/"Save", by mode) -- e.g. the
  // Asset Browser's "Import from Disk..." picker uses "Import" so the button reads right for
  // what it actually does, without a caller having to double-click just to proceed.
  std::string confirm_label{};
}; // struct file_dialog_options

/**
 * @brief A Blender-style, ImGui-only file/folder picker — no native OS dialog, no third-party
 * dependency. Lives here (not in `editor/`) because it's shared between
 * `editor::asset_browser_panel`, `editor::editor_ui_layer` (Save Scene As), and the launcher's
 * project picker.
 *
 * Not a `ui_layer` — a plain widget any layer's `build()` calls into. Usage: `open(...)` to
 * trigger it, `draw()` once per frame while `is_open()`, poll `result()`.
 */
class file_dialog final : public utility::noncopyable {

public:

  file_dialog() = default;

  auto open(file_dialog_options options) -> void;

  /** @brief Draws the dialog. Call once per frame while is_open() is true. */
  auto draw() -> void;

  [[nodiscard]] auto is_open() const noexcept -> bool {
    return _is_open;
  }

  /**
   * @brief Non-nullopt exactly once, the frame the user confirms or cancels — an empty vector
   * means cancelled (or confirmed with nothing selected, which the confirm button already
   * disallows in open_file/open_files/save_file mode). Consuming: returns nullopt again afterward,
   * until the dialog produces another result.
   */
  [[nodiscard]] auto result() -> std::optional<std::vector<std::filesystem::path>>;

private:

  struct entry {
    std::filesystem::path path{};
    bool is_directory{false};
  }; // struct entry

  auto _navigate_to(std::filesystem::path directory) -> void;
  auto _refresh_entries() -> void;

  auto _draw_breadcrumbs() -> void;
  auto _draw_shortcuts_sidebar() -> void;
  auto _draw_listing() -> void;
  auto _draw_entry_row(std::size_t index, const std::vector<std::size_t>& visible) -> void;
  auto _handle_keyboard_navigation(const std::vector<std::size_t>& visible) -> void;

  auto _draw_new_folder_popup() -> void;
  auto _draw_overwrite_confirm_popup() -> void;

  auto _select_only(std::size_t index) -> void;
  auto _toggle_selection(std::size_t index) -> void;
  auto _select_range(std::size_t anchor, std::size_t clicked, const std::vector<std::size_t>& visible) -> void;

  [[nodiscard]] auto _can_confirm() const -> bool;
  auto _confirm() -> void;
  auto _try_confirm_save() -> void;
  auto _finalize_save(const std::filesystem::path& path) -> void;
  auto _cancel() -> void;

  file_dialog_options _options{};
  std::string _popup_id{};

  bool _is_open{false};
  bool _should_open_popup{false};

  std::filesystem::path _current_directory{};

  std::vector<entry> _cached_entries{};
  std::vector<bool> _entry_selected{}; // parallel to _cached_entries; files only.
  std::optional<std::size_t> _selection_anchor{}; // last plain-clicked index, into _cached_entries -- Shift range-select's anchor
  std::optional<std::size_t> _focused_index{}; // keyboard-navigable cursor row, into _cached_entries
  bool _needs_refresh{true};

  std::array<char, 256u> _filter_buffer{};

  bool _show_new_folder_popup{false};
  std::array<char, 128u> _new_folder_name_buffer{};

  std::array<char, 256u> _save_file_name_buffer{}; // save_file mode only

  bool _show_overwrite_confirm_popup{false};
  std::filesystem::path _pending_overwrite_path{};

  std::optional<std::vector<std::filesystem::path>> _result{};

}; // class file_dialog

} // namespace sbx::render

#endif // LIBSBX_RENDER_UI_WIDGETS_FILE_DIALOG_HPP_
