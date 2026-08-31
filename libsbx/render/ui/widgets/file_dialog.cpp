// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/ui/widgets/file_dialog.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <imgui.h>

#include <libsbx/utility/target.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

namespace sbx::render::widgets {

namespace {

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

} // namespace

auto file_dialog::open(std::string title, file_dialog_mode mode, const std::filesystem::path& start_dir, std::vector<std::string> extensions) -> void {
  _title = std::move(title);
  _popup_id = _title + "##file_dialog";
  _mode = mode;
  _extensions = std::move(extensions);

  auto ec = std::error_code{};
  _current_directory = (!start_dir.empty() && std::filesystem::is_directory(start_dir, ec)) ? start_dir : home_directory();

  _is_open = true;
  _should_open_popup = true;
  _needs_refresh = true;
  _entry_selected.clear();
  _result.reset();
}

auto file_dialog::result() -> std::optional<std::vector<std::filesystem::path>> {
  auto taken = std::move(_result);
  _result.reset();
  return taken;
}

auto file_dialog::_refresh_entries() -> void {
  _cached_entries.clear();

  auto ec = std::error_code{};

  if (!std::filesystem::is_directory(_current_directory, ec)) {
    _needs_refresh = false;
    return;
  }

  for (const auto& dir_entry : std::filesystem::directory_iterator{_current_directory, ec}) {
    const auto is_directory = dir_entry.is_directory(ec);

    if (!is_directory) {
      if (_mode == file_dialog_mode::select_folder) {
        continue; // folder mode only ever lists directories — nothing else could ever be picked.
      }

      if (!_extensions.empty() && std::ranges::find(_extensions, dir_entry.path().extension().string()) == _extensions.end()) {
        continue;
      }
    }

    _cached_entries.push_back(entry{dir_entry.path(), is_directory});
  }

  std::ranges::sort(_cached_entries, [](const auto& lhs, const auto& rhs) {
    if (lhs.is_directory != rhs.is_directory) {
      return lhs.is_directory > rhs.is_directory;
    }

    return lhs.path.filename() < rhs.path.filename();
  });

  _entry_selected.assign(_cached_entries.size(), false);

  _needs_refresh = false;
}

auto file_dialog::_confirm() -> void {
  auto picked = std::vector<std::filesystem::path>{};

  if (_mode == file_dialog_mode::select_folder) {
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

auto file_dialog::_cancel() -> void {
  _result = std::vector<std::filesystem::path>{};
  _is_open = false;
}

auto file_dialog::draw() -> void {
  if (!_is_open) {
    return;
  }

  if (_should_open_popup) {
    ImGui::OpenPopup(_popup_id.c_str());
    _should_open_popup = false;
  }

  ImGui::SetNextWindowSize(ImVec2{640.0f, 480.0f}, ImGuiCond_FirstUseEver);

  auto still_open = true;

  if (!ImGui::BeginPopupModal(_popup_id.c_str(), &still_open, ImGuiWindowFlags_NoSavedSettings)) {
    return; // OpenPopup was only just requested this frame — nothing to draw yet.
  }

  if (_needs_refresh) {
    _refresh_entries();
  }

  // Breadcrumb / path bar: "up one level" button, then an editable path (Enter navigates).
  const auto can_go_up = _current_directory.has_relative_path() && _current_directory != _current_directory.root_path();

  ImGui::BeginDisabled(!can_go_up);

  if (ImGui::Button(ICON_MDI_ARROW_UP)) {
    _current_directory = _current_directory.parent_path();
    _needs_refresh = true;
  }

  ImGui::EndDisabled();

  ImGui::SameLine();

  const auto path_string = _current_directory.generic_string();
  const auto copy_length = std::min(path_string.size(), _path_buffer.size() - 1u);
  std::memcpy(_path_buffer.data(), path_string.data(), copy_length);
  _path_buffer[copy_length] = '\0';

  ImGui::SetNextItemWidth(-1.0f);

  if (ImGui::InputText("##file_dialog_path", _path_buffer.data(), _path_buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue)) {
    auto ec = std::error_code{};
    auto candidate = std::filesystem::path{_path_buffer.data()};

    if (std::filesystem::is_directory(candidate, ec)) {
      _current_directory = std::move(candidate);
      _needs_refresh = true;
    }
  }

  // Listing — leaves room below for the confirm/cancel row.
  const auto listing_height = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing();

  if (ImGui::BeginChild("##file_dialog_listing", ImVec2{0.0f, listing_height}, ImGuiChildFlags_Borders)) {
    if (_cached_entries.empty()) {
      ImGui::TextDisabled("Nothing here.");
    }

    for (auto index = std::size_t{0u}; index < _cached_entries.size() && _is_open; ++index) {
      auto& item = _cached_entries[index];

      ImGui::PushID(static_cast<std::int32_t>(index));

      const auto label = std::string{item.is_directory ? ICON_MDI_FOLDER : ICON_MDI_FILE_OUTLINE} + " " + item.path.filename().string();
      const auto is_selected = !item.is_directory && index < _entry_selected.size() && _entry_selected[index];

      if (ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
        if (item.is_directory) {
          // Directories are always for navigation, in every mode — select_folder confirms
          // whatever directory is currently being browsed (below), not a row in its listing.
          _current_directory = item.path;
          _needs_refresh = true;
        } else if (_mode == file_dialog_mode::open_files) {
          _entry_selected[index] = !_entry_selected[index];
        } else if (_mode == file_dialog_mode::open_file) {
          std::ranges::fill(_entry_selected, false);
          _entry_selected[index] = true;

          if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            _confirm();
          }
        }
      }

      ImGui::PopID();
    }
  }

  ImGui::EndChild();

  const auto has_file_selection = std::ranges::any_of(_entry_selected, [](bool selected) { return selected; });
  const auto can_confirm = _mode == file_dialog_mode::select_folder || has_file_selection;

  ImGui::BeginDisabled(!can_confirm);

  if (ImGui::Button(_mode == file_dialog_mode::select_folder ? "Select" : "Open")) {
    _confirm();
  }

  ImGui::EndDisabled();

  ImGui::SameLine();

  if (ImGui::Button("Cancel")) {
    _cancel();
  }

  if (!still_open) {
    _cancel();
  }

  if (!_is_open) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

} // namespace sbx::render::widgets
