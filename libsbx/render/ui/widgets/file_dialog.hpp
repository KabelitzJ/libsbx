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
  select_folder, // no file selection needed to confirm
  save_file      // type/pick a filename; confirms through an overwrite prompt if it already exists
}; // enum class file_dialog_mode

/** @brief A named shortcut in the dialog's sidebar, next to the built-in "Home". */
struct file_dialog_shortcut {
  std::string label{};
  std::filesystem::path path{};
}; // struct file_dialog_shortcut

struct file_dialog_options {
  std::string title{};
  file_dialog_mode mode{file_dialog_mode::open_file};

  // Falls back to the home directory if empty or missing.
  std::filesystem::path start_dir{};

  // Exact extensions with the leading dot (".gltf"); empty means no filter. Ignored by select_folder. save_file appends extensions.front() when missing.
  std::vector<std::string> extensions{};

  std::vector<file_dialog_shortcut> shortcuts{};

  // save_file only: seeds the file name field.
  std::string default_file_name{};

  // Overrides the confirm button's label (default "Open", "Select" or "Save" by mode).
  std::string confirm_label{};
}; // struct file_dialog_options

/**
 * @brief An ImGui-only file and folder picker, shared by the editor and the launcher.
 *
 * Not a ui_layer: call open(), then draw() every frame while is_open(), and poll result().
 */
class file_dialog final : public utility::noncopyable {

public:

  file_dialog() = default;

  auto open(file_dialog_options options) -> void;

  auto draw() -> void;

  [[nodiscard]] auto is_open() const noexcept -> bool {
    return _is_open;
  }

  /**
   * @brief The dialog's outcome, returned once on the frame the user confirms or cancels.
   *
   * @return The chosen paths (empty when cancelled), or nullopt on every other frame.
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
  std::optional<std::size_t> _selection_anchor{}; // Shift range-select anchor, into _cached_entries
  std::optional<std::size_t> _focused_index{}; // keyboard cursor row, into _cached_entries
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
