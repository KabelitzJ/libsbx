// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/ui/widgets/file_dialog.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>

#include <imgui.h>

#include <libsbx/utility/target.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

namespace sbx::render {

[[nodiscard]] auto home_directory() -> std::filesystem::path {
#if defined(SBX_PLATFORM_WIN32)
  if (auto* profile = std::getenv("USERPROFILE")) {
    return std::filesystem::path{profile};
  }
#else
  if (auto* home = std::getenv("HOME")) {
    return std::filesystem::path{home};
  }
#endif

  return std::filesystem::current_path();
}

// Case-insensitive alphabetical order, dirs-first sort key.
[[nodiscard]] auto filename_less(const std::filesystem::path& lhs, const std::filesystem::path& rhs) -> bool {
  const auto to_lower = [](const std::filesystem::path& path) {
    auto name = path.filename().string();
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return std::tolower(c); });
    return name;
  };

  return to_lower(lhs) < to_lower(rhs);
}

// Dotfiles/dotfolders never show up in the listing -- same convention as the editor's Asset
// Browser, and what every native file manager does by default.
[[nodiscard]] auto is_hidden_entry(const std::filesystem::path& name) -> bool {
  const auto name_string = name.string();
  return !name_string.empty() && name_string.front() == '.';
}

// Case-insensitive substring test for the search box -- an empty filter matches everything.
[[nodiscard]] auto filter_matches(std::string_view name, std::string_view filter) -> bool {
  if (filter.empty()) {
    return true;
  }

  const auto to_lower = [](std::string_view text) {
    auto result = std::string{text};
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
  };

  return to_lower(name).find(to_lower(filter)) != std::string::npos;
}

auto file_dialog::open(file_dialog_options options) -> void {
  _options = std::move(options);
  _popup_id = _options.title + "##file_dialog";

  auto ec = std::error_code{};
  _current_directory = (!_options.start_dir.empty() && std::filesystem::is_directory(_options.start_dir, ec)) ? _options.start_dir : home_directory();

  _is_open = true;
  _should_open_popup = true;
  _needs_refresh = true;

  _entry_selected.clear();
  _selection_anchor.reset();
  _focused_index.reset();

  _filter_buffer.fill('\0');

  _save_file_name_buffer.fill('\0');

  if (!_options.default_file_name.empty()) {
    std::strncpy(_save_file_name_buffer.data(), _options.default_file_name.c_str(), _save_file_name_buffer.size() - 1u);
  }

  _show_new_folder_popup = false;
  _show_overwrite_confirm_popup = false;

  _result.reset();
}

auto file_dialog::result() -> std::optional<std::vector<std::filesystem::path>> {
  auto taken = std::move(_result);
  _result.reset();
  return taken;
}

auto file_dialog::_navigate_to(std::filesystem::path directory) -> void {
  _current_directory = std::move(directory);
  _needs_refresh = true;
  _entry_selected.clear();
  _selection_anchor.reset();
  _focused_index.reset();
}

auto file_dialog::_refresh_entries() -> void {
  _cached_entries.clear();

  auto ec = std::error_code{};

  if (!std::filesystem::is_directory(_current_directory, ec)) {
    _needs_refresh = false;
    return;
  }

  for (const auto& dir_entry : std::filesystem::directory_iterator{_current_directory, ec}) {
    if (is_hidden_entry(dir_entry.path().filename())) {
      continue;
    }

    const auto is_directory = dir_entry.is_directory(ec);

    if (!is_directory) {
      if (_options.mode == file_dialog_mode::select_folder) {
        continue; // folder mode only ever lists directories — nothing else could ever be picked.
      }

      if (!_options.extensions.empty() && std::ranges::find(_options.extensions, dir_entry.path().extension().string()) == _options.extensions.end()) {
        continue;
      }
    }

    _cached_entries.push_back(entry{dir_entry.path(), is_directory});
  }

  std::ranges::sort(_cached_entries, [](const auto& lhs, const auto& rhs) {
    if (lhs.is_directory != rhs.is_directory) {
      return lhs.is_directory > rhs.is_directory;
    }

    return filename_less(lhs.path, rhs.path);
  });

  _entry_selected.assign(_cached_entries.size(), false);
  _selection_anchor.reset();
  _focused_index.reset();

  _needs_refresh = false;
}

auto file_dialog::_select_only(std::size_t index) -> void {
  std::ranges::fill(_entry_selected, false);

  if (index < _entry_selected.size()) {
    _entry_selected[index] = true;
  }
}

auto file_dialog::_toggle_selection(std::size_t index) -> void {
  if (index < _entry_selected.size()) {
    _entry_selected[index] = !_entry_selected[index];
  }
}

auto file_dialog::_select_range(std::size_t anchor, std::size_t clicked, const std::vector<std::size_t>& visible) -> void {
  const auto anchor_it = std::ranges::find(visible, anchor);
  const auto clicked_it = std::ranges::find(visible, clicked);

  if (anchor_it == visible.end() || clicked_it == visible.end()) {
    _select_only(clicked);
    return;
  }

  std::ranges::fill(_entry_selected, false);

  const auto [low, high] = std::minmax(anchor_it, clicked_it);

  for (auto it = low; it != high + 1; ++it) {
    if (*it < _entry_selected.size()) {
      _entry_selected[*it] = true;
    }
  }
}

auto file_dialog::_can_confirm() const -> bool {
  switch (_options.mode) {
    case file_dialog_mode::select_folder:
      return true;
    case file_dialog_mode::save_file:
      return _save_file_name_buffer[0] != '\0';
    case file_dialog_mode::open_file:
    case file_dialog_mode::open_files:
      return std::ranges::any_of(_entry_selected, [](bool selected) { return selected; });
  }

  return false;
}

auto file_dialog::_confirm() -> void {
  auto picked = std::vector<std::filesystem::path>{};

  if (_options.mode == file_dialog_mode::select_folder) {
    picked.push_back(_current_directory);
  } else {
    for (auto index = std::size_t{0u}; index < _cached_entries.size(); ++index) {
      if (index < _entry_selected.size() && _entry_selected[index]) {
        picked.push_back(_cached_entries[index].path);
      }
    }
  }

  _result = std::move(picked);
  _is_open = false;
}

auto file_dialog::_try_confirm_save() -> void {
  const auto typed = std::string{_save_file_name_buffer.data()};

  if (typed.empty()) {
    return;
  }

  auto candidate = std::filesystem::path{typed};

  if (!_options.extensions.empty() && candidate.extension().empty()) {
    candidate += _options.extensions.front();
  }

  const auto target = _current_directory / candidate;

  auto ec = std::error_code{};

  if (std::filesystem::exists(target, ec)) {
    _pending_overwrite_path = target;
    _show_overwrite_confirm_popup = true;
  } else {
    _finalize_save(target);
  }
}

auto file_dialog::_finalize_save(const std::filesystem::path& path) -> void {
  _result = std::vector<std::filesystem::path>{path};
  _is_open = false;
}

auto file_dialog::_cancel() -> void {
  _result = std::vector<std::filesystem::path>{};
  _is_open = false;
}

auto file_dialog::_draw_breadcrumbs() -> void {
  const auto can_go_up = _current_directory.has_relative_path() && _current_directory != _current_directory.root_path();

  ImGui::BeginDisabled(!can_go_up);

  if (ImGui::Button(ICON_MDI_ARROW_UP)) {
    _navigate_to(_current_directory.parent_path());
  }

  ImGui::EndDisabled();

  ImGui::SameLine();

  // Root segment (drive root on Windows, "/" elsewhere), then one clickable button per path
  // component after it — clicking any segment jumps straight there.
  auto accumulated = _current_directory.root_path();

  ImGui::PushID("##file_dialog_breadcrumb_root");

  if (ImGui::Button(accumulated.empty() ? "/" : accumulated.string().c_str())) {
    _navigate_to(accumulated);
  }

  ImGui::PopID();

  for (const auto& segment : _current_directory.lexically_relative(accumulated)) {
    if (segment == ".") {
      continue;
    }

    accumulated /= segment;

    ImGui::SameLine(0.0f, 4.0f);
    ImGui::TextDisabled("%s", ICON_MDI_CHEVRON_RIGHT);
    ImGui::SameLine(0.0f, 4.0f);

    ImGui::PushID(accumulated.string().c_str());

    if (ImGui::Button(segment.string().c_str())) {
      _navigate_to(accumulated);
    }

    ImGui::PopID();
  }

  // New Folder, right-aligned on this same row -- available in every mode, same as a native
  // dialog's own toolbar.
  const auto new_folder_label = std::string{ICON_MDI_FOLDER_PLUS} + " New Folder";
  const auto button_width = ImGui::CalcTextSize(new_folder_label.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
  const auto avail = ImGui::GetContentRegionAvail().x;

  if (avail > button_width) {
    ImGui::SameLine(ImGui::GetCursorPosX() + avail - button_width);
  } else {
    ImGui::SameLine();
  }

  if (ImGui::Button(new_folder_label.c_str())) {
    _show_new_folder_popup = true;
  }
}

auto file_dialog::_draw_shortcuts_sidebar() -> void {
  const auto home_label = std::string{ICON_MDI_HOME} + " Home";

  if (ImGui::Selectable(home_label.c_str())) {
    _navigate_to(home_directory());
  }

  for (const auto& shortcut : _options.shortcuts) {
    const auto label = std::string{ICON_MDI_FOLDER} + " " + shortcut.label;

    ImGui::PushID(shortcut.label.c_str());

    if (ImGui::Selectable(label.c_str())) {
      _navigate_to(shortcut.path);
    }

    ImGui::PopID();
  }
}

auto file_dialog::_handle_keyboard_navigation(const std::vector<std::size_t>& visible) -> void {
  // Never fights the search box or filename field's own text editing/cursor movement.
  if (visible.empty() || ImGui::GetIO().WantTextInput) {
    return;
  }

  auto position = std::optional<std::size_t>{};

  if (_focused_index) {
    if (const auto it = std::ranges::find(visible, *_focused_index); it != visible.end()) {
      position = static_cast<std::size_t>(it - visible.begin());
    }
  }

  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
    position = position ? std::min(*position + 1u, visible.size() - 1u) : std::size_t{0u};
    _focused_index = visible[*position];
  } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
    position = (position && *position > 0u) ? *position - 1u : std::size_t{0u};
    _focused_index = visible[*position];
  }

  if (!_focused_index || !ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
    return;
  }

  const auto& item = _cached_entries[*_focused_index];

  if (item.is_directory) {
    _navigate_to(item.path);
    return;
  }

  switch (_options.mode) {
    case file_dialog_mode::open_file: {
      _select_only(*_focused_index);
      _confirm();
      break;
    }
    case file_dialog_mode::save_file: {
      const auto name = item.path.filename().string();
      std::strncpy(_save_file_name_buffer.data(), name.c_str(), _save_file_name_buffer.size() - 1u);
      _try_confirm_save();
      break;
    }
    case file_dialog_mode::open_files: {
      _toggle_selection(*_focused_index);
      break;
    }
    case file_dialog_mode::select_folder: {
      break; // no file rows exist in this mode
    }
  }
}

auto file_dialog::_draw_entry_row(std::size_t index, const std::vector<std::size_t>& visible) -> void {
  auto& item = _cached_entries[index];

  ImGui::PushID(static_cast<std::int32_t>(index));

  const auto label = std::string{item.is_directory ? ICON_MDI_FOLDER : ICON_MDI_FILE_OUTLINE} + " " + item.path.filename().string();
  const auto is_selected = _focused_index == index || (!item.is_directory && index < _entry_selected.size() && _entry_selected[index]);

  if (ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
    _focused_index = index;

    if (item.is_directory) {
      _navigate_to(item.path);
    } else if (_options.mode == file_dialog_mode::save_file) {
      const auto name = item.path.filename().string();
      std::strncpy(_save_file_name_buffer.data(), name.c_str(), _save_file_name_buffer.size() - 1u);
    } else if (_options.mode == file_dialog_mode::open_files && ImGui::GetIO().KeyShift && _selection_anchor) {
      _select_range(*_selection_anchor, index, visible);
    } else if (_options.mode == file_dialog_mode::open_files && ImGui::GetIO().KeyCtrl) {
      _toggle_selection(index);
      _selection_anchor = index;
    } else {
      const auto double_clicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

      _select_only(index);
      _selection_anchor = index;

      if (double_clicked) {
        _confirm();
      }
    }
  }

  ImGui::PopID();
}

auto file_dialog::_draw_listing() -> void {
  auto visible = std::vector<std::size_t>{};

  for (auto index = std::size_t{0u}; index < _cached_entries.size(); ++index) {
    if (filter_matches(_cached_entries[index].path.filename().string(), _filter_buffer.data())) {
      visible.push_back(index);
    }
  }

  if (visible.empty()) {
    ImGui::TextDisabled("Nothing here.");
    return;
  }

  _handle_keyboard_navigation(visible);

  auto clipper = ImGuiListClipper{};
  clipper.Begin(static_cast<std::int32_t>(visible.size()));

  while (clipper.Step()) {
    for (auto row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      _draw_entry_row(visible[static_cast<std::size_t>(row)], visible);
    }
  }
}

auto file_dialog::_draw_new_folder_popup() -> void {
  if (_show_new_folder_popup) {
    ImGui::OpenPopup("New Folder");
    _show_new_folder_popup = false;

    _new_folder_name_buffer.fill('\0');
    std::strncpy(_new_folder_name_buffer.data(), "New Folder", _new_folder_name_buffer.size() - 1u);
  }

  if (!ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }

  ImGui::SetNextItemWidth(240.0f);
  ImGui::InputText("##new_folder_name", _new_folder_name_buffer.data(), _new_folder_name_buffer.size(), ImGuiInputTextFlags_AutoSelectAll);

  const auto typed = std::filesystem::path{_new_folder_name_buffer.data()};
  const auto is_empty = typed.empty();

  auto ec = std::error_code{};
  const auto collides = !is_empty && std::filesystem::exists(_current_directory / typed, ec);

  if (is_empty) {
    ImGui::TextDisabled("Enter a name.");
  } else if (collides) {
    ImGui::TextColored(ImVec4{1.0f, 0.7f, 0.2f, 1.0f}, "A folder with this name already exists.");
  } else {
    ImGui::Dummy(ImVec2{0.0f, ImGui::GetTextLineHeight()});
  }

  ImGui::BeginDisabled(is_empty || collides);

  if (ImGui::Button("Create")) {
    std::filesystem::create_directory(_current_directory / typed, ec);
    _needs_refresh = true;
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndDisabled();

  ImGui::SameLine();

  if (ImGui::Button("Cancel")) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

auto file_dialog::_draw_overwrite_confirm_popup() -> void {
  if (_show_overwrite_confirm_popup) {
    ImGui::OpenPopup("Overwrite File?");
    _show_overwrite_confirm_popup = false;
  }

  if (!ImGui::BeginPopupModal("Overwrite File?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::Text("'%s' already exists. Overwrite it?", _pending_overwrite_path.filename().string().c_str());

  if (ImGui::Button("Overwrite")) {
    ImGui::CloseCurrentPopup();
    _finalize_save(_pending_overwrite_path);
  }

  ImGui::SameLine();

  if (ImGui::Button("Cancel")) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

auto file_dialog::draw() -> void {
  if (!_is_open) {
    return;
  }

  if (_should_open_popup) {
    ImGui::OpenPopup(_popup_id.c_str());
    _should_open_popup = false;
  }

  ImGui::SetNextWindowSize(ImVec2{760.0f, 520.0f}, ImGuiCond_FirstUseEver);

  auto still_open = true;

  if (!ImGui::BeginPopupModal(_popup_id.c_str(), &still_open, ImGuiWindowFlags_NoSavedSettings)) {
    return; // OpenPopup was only just requested this frame — nothing to draw yet.
  }

  if (_needs_refresh) {
    _refresh_entries();
  }

  _draw_breadcrumbs();

  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }

  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint("##file_dialog_filter", ICON_MDI_MAGNIFY " Search...", _filter_buffer.data(), _filter_buffer.size());

  // Leaves room below for the filename field (save_file mode only) and the confirm/cancel row.
  const auto bottom_rows = (_options.mode == file_dialog_mode::save_file) ? 2.0f : 1.0f;
  const auto body_height = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * bottom_rows;

  if (ImGui::BeginTable("##file_dialog_columns", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2{0.0f, body_height})) {
    ImGui::TableSetupColumn("Shortcuts", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn("Listing", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::BeginChild("##file_dialog_shortcuts");
    _draw_shortcuts_sidebar();
    ImGui::EndChild();

    ImGui::TableSetColumnIndex(1);
    ImGui::BeginChild("##file_dialog_listing", ImVec2{0.0f, 0.0f}, ImGuiChildFlags_Borders);
    _draw_listing();
    ImGui::EndChild();

    ImGui::EndTable();
  }

  if (_options.mode == file_dialog_mode::save_file) {
    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::InputText("##file_dialog_save_name", _save_file_name_buffer.data(), _save_file_name_buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
      _try_confirm_save();
    }
  }

  ImGui::BeginDisabled(!_can_confirm());

  const auto* default_label = _options.mode == file_dialog_mode::select_folder ? "Select" : _options.mode == file_dialog_mode::save_file ? "Save" : "Open";
  const auto confirm_label = _options.confirm_label.empty() ? std::string{default_label} : _options.confirm_label;

  if (ImGui::Button(confirm_label.c_str())) {
    if (_options.mode == file_dialog_mode::save_file) {
      _try_confirm_save();
    } else {
      _confirm();
    }
  }

  ImGui::EndDisabled();

  ImGui::SameLine();

  if (ImGui::Button("Cancel")) {
    _cancel();
  }

  _draw_new_folder_popup();
  _draw_overwrite_confirm_popup();

  if (!still_open) {
    _cancel();
  }

  if (!_is_open) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

} // namespace sbx::render
