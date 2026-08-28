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

namespace sbx::render::widgets {

enum class file_dialog_mode : std::uint8_t {
  open_file,     // single file, extension-filtered
  open_files,    // multi-select files, extension-filtered
  select_folder  // pick a directory; no file selection required to confirm
}; // enum class file_dialog_mode

/**
 * @brief A Blender-style, ImGui-only file/folder picker — no native OS dialog, no third-party
 * dependency. Lives here (not in `editor/`) because it's shared between
 * `editor::asset_browser_panel` and a future launcher's project picker.
 *
 * Not a `ui_layer` — a plain widget any layer's `build()` calls into. Usage: `open(...)` to
 * trigger it, `draw()` once per frame while `is_open()`, poll `result()`.
 */
class file_dialog final : public utility::noncopyable {

public:

  file_dialog() = default;

  /**
   * @brief Opens the dialog. @p start_dir falls back to the user's home directory if empty or
   * not an existing directory. @p extensions filters the listing in open_file/open_files mode
   * (each entry including its leading dot, e.g. ".gltf", matched exactly against
   * `path::extension()` the same way `asset_browser_panel::classify_extension` does); empty
   * means no filtering. Ignored in select_folder mode, which only ever lists directories.
   */
  auto open(std::string title, file_dialog_mode mode, const std::filesystem::path& start_dir = {}, std::vector<std::string> extensions = {}) -> void;

  /** @brief Draws the dialog. Call once per frame while is_open() is true. */
  auto draw() -> void;

  [[nodiscard]] auto is_open() const noexcept -> bool {
    return _is_open;
  }

  /**
   * @brief Non-nullopt exactly once, the frame the user confirms or cancels — an empty vector
   * means cancelled (or confirmed with nothing selected, which the confirm button already
   * disallows in open_file/open_files mode). Consuming: returns nullopt again afterward, until
   * the dialog produces another result.
   */
  [[nodiscard]] auto result() -> std::optional<std::vector<std::filesystem::path>>;

private:

  struct entry {
    std::filesystem::path path{};
    bool is_directory{false};
  }; // struct entry

  auto _refresh_entries() -> void;

  auto _confirm() -> void;

  auto _cancel() -> void;

  std::string _title{};
  std::string _popup_id{};
  file_dialog_mode _mode{file_dialog_mode::open_file};
  std::vector<std::string> _extensions{};

  bool _is_open{false};
  bool _should_open_popup{false};

  std::filesystem::path _current_directory{};
  std::array<char, 1024u> _path_buffer{};

  std::vector<entry> _cached_entries{};
  std::vector<bool> _entry_selected{}; // parallel to _cached_entries; files only.
  bool _needs_refresh{true};

  std::optional<std::vector<std::filesystem::path>> _result{};

}; // class file_dialog

} // namespace sbx::render::widgets

#endif // LIBSBX_RENDER_UI_WIDGETS_FILE_DIALOG_HPP_
