// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/import_window.hpp>

#include <libsbx/utility/profiler.hpp>

#include <algorithm>
#include <string>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

namespace editor {

inline constexpr auto import_popup_id = "###import_window";

auto import_window::draw(editor_state& state) -> void {
  SBX_PROFILE_SCOPE("import_window::draw");

  if (!_current.has_value()) {
    _advance(state);
  }

  if (!_current.has_value()) {
    return;
  }

  if (!ImGui::IsPopupOpen(import_popup_id)) {
    ImGui::OpenPopup(import_popup_id);
  }

  const auto title = fmt::format("{} Import {}: {}{}", ICON_MDI_FILE_IMPORT, sbx::assets::display_name(_current->type()), _current->relative_path().filename().string(), import_popup_id);

  ImGui::SetNextWindowSize(ImVec2{420.0f, 0.0f}, ImGuiCond_Appearing);

  if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::TextDisabled("%s", _current->relative_path().generic_string().c_str());

  if (!state.import_requests.empty()) {
    ImGui::TextDisabled("%zu more file(s) queued after this one.", state.import_requests.size());
  }

  ImGui::Separator();

  _current->draw();

  ImGui::Separator();

  const auto remaining_of_type = std::ranges::count_if(state.import_requests, [this](const auto& path) { return sbx::assets::asset_type_of(path) == _current->type(); });

  if (remaining_of_type > 0) {
    const auto label = fmt::format("Apply to the {} remaining {} file(s)", remaining_of_type, sbx::assets::display_name(_current->type()));
    ImGui::Checkbox(label.c_str(), &_apply_to_remaining);
  }

  if (ImGui::Button(ICON_MDI_CHECK " Import")) {
    _import_current(state);
    ImGui::CloseCurrentPopup();
  }

  ImGui::SameLine();

  if (ImGui::Button("Skip")) {
    _current.reset();
    ImGui::CloseCurrentPopup();
  }

  ImGui::SameLine();

  if (ImGui::Button("Skip All")) {
    _current.reset();
    state.import_requests.clear();
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

auto import_window::_import_current(editor_state& state) -> void {
  const auto settings = _current->settings();
  const auto type = _current->type();
  const auto relative_path = _current->relative_path();

  const auto id = _current->apply();
  state.select_asset(id, relative_path, type);
  state.asset_browser_refresh_request = true; // the tile can carry its new uuid now

  _current.reset();

  if (!_apply_to_remaining) {
    return;
  }

  // Consumes every queued file of the same type with these settings, leaving the others queued in order.
  std::erase_if(state.import_requests, [&](const std::filesystem::path& path) {
    if (sbx::assets::asset_type_of(path) != type) {
      return false;
    }

    auto editor = import_settings_editor{path};
    editor.adopt(settings);
    editor.apply();

    return true;
  });

  _apply_to_remaining = false;
}

// Pops requests until one needs the window; files of types without an importer are imported on the spot.
auto import_window::_advance(editor_state& state) -> void {
  auto& project = sbx::core::engine::project();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  while (!state.import_requests.empty() && !_current.has_value()) {
    auto path = std::move(state.import_requests.front());
    state.import_requests.erase(state.import_requests.begin());

    const auto absolute_path = project.assets_directory() / path;
    const auto type = sbx::assets::asset_type_of(path);

    if (!std::filesystem::exists(absolute_path) || !sbx::assets::is_manifested(type)) {
      continue;
    }

    if (!sbx::assets::has_import_settings(type)) {
      assets_module.import(absolute_path);
      continue;
    }

    _current.emplace(std::move(path));
  }
}

} // namespace editor
