// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_module.hpp>

#include <imgui.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/physics/physics_module.hpp>

namespace editor {

editor_module::editor_module()
: _ini_file{(sbx::core::engine::project().root() / ".sbx" / "editor" / "imgui.ini").string()} {
  std::filesystem::create_directories(std::filesystem::path{_ini_file}.parent_path());

  ImGui::GetIO().IniFilename = nullptr;

  ImGui::LoadIniSettingsFromDisk(_ini_file.data());

  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();
  auto& scene_renderer_module = sbx::core::engine::get_module<sbx::render::scene_renderer_module>();
  auto& physics_module = sbx::core::engine::get_module<sbx::physics::physics_module>();

  ui_module.add_layer(&_ui_layer);
  scene_renderer_module.set_grid_enabled(true);
  physics_module.set_debug_draw_flags(sbx::physics::debug_draw_flags{.colliders = true});
}

editor_module::~editor_module() {
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  ui_module.remove_layer(&_ui_layer);

  ImGui::SaveIniSettingsToDisk(_ini_file.c_str());
}

} // namespace editor
