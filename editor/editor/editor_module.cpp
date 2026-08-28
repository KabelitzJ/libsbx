// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_module.hpp>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

namespace editor {

editor_module::editor_module()
: _ini_file{(sbx::core::engine::project().root() / ".sbx" / "editor" / "imgui.ini").string()} {
  std::filesystem::create_directories(std::filesystem::path{_ini_file}.parent_path());

  ImGui::GetIO().IniFilename = nullptr;

  ImGui::LoadIniSettingsFromDisk(_ini_file.data());

  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();

  render_module.ui().add_layer(&_ui_layer);
  render_module.set_grid_enabled(true);
}

editor_module::~editor_module() {
  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();

  render_module.ui().remove_layer(&_ui_layer);

  ImGui::SaveIniSettingsToDisk(_ini_file.c_str());
}

} // namespace editor
