// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <launcher/launcher_module.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/target.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/process.hpp>
#include <libsbx/platform/window.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

namespace launcher {

namespace {

/** @brief Resolved next to the launcher's own binary — editor/runtime/launcher all share RUNTIME_OUTPUT_DIRECTORY (see their respective CMakeLists.txt). */
[[nodiscard]] auto executable_path(launcher_module::launch_target target) -> std::filesystem::path {
  const auto* name =
#if defined(SBX_PLATFORM_WIN32)
    target == launcher_module::launch_target::editor ? "editor.exe" : "runtime.exe";
#else
    target == launcher_module::launch_target::editor ? "editor" : "runtime";
#endif

  return sbx::filesystem::executable_directory() / name;
}

} // namespace

launcher_module::launcher_module() {
  auto& platform_module = sbx::core::engine::get_module<sbx::platform::platform_module>();
  auto& window = platform_module.window();

  window.on_window_closed() += [](const auto&) {
    sbx::core::engine::quit();
  };

  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  ImGui::GetIO().IniFilename = nullptr;

  ui_module.add_default_fonts(16.0f);
  ui_module.apply_default_style();
  ui_module.add_layer(this);
}

launcher_module::~launcher_module() {
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();
  ui_module.remove_layer(this);
}

auto launcher_module::build() -> void {
  const auto* viewport = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);

  ImGui::Begin("libsbx", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

  ImGui::Text("Projects");
  ImGui::Separator();

  if (ImGui::Button("New Project...")) {
    _pending_pick = pending_pick::new_project_parent;
    _file_dialog.open("New Project - Choose a Location", sbx::render::widgets::file_dialog_mode::select_folder);
  }

  ImGui::SameLine();

  if (ImGui::Button("Open Project...")) {
    _pending_pick = pending_pick::open_project;

    auto extensions = std::vector<std::string>{};
    extensions.push_back(".sbxproj");

    _file_dialog.open("Open Project", sbx::render::widgets::file_dialog_mode::open_file, {}, extensions);
  }

  ImGui::Separator();

  _draw_recent_projects();

  ImGui::End();

  _file_dialog.draw();

  if (auto picked = _file_dialog.result(); picked && !picked->empty()) {
    const auto picked_path = picked->front();
    const auto pending = std::exchange(_pending_pick, pending_pick::none);

    if (pending == pending_pick::open_project) {
      _launch(launch_target::editor, picked_path.parent_path());
    } else if (pending == pending_pick::new_project_parent) {
      _new_project_parent = picked_path;
      _show_new_project_name_dialog = true;
    }
  }

  _draw_new_project_name_dialog();
}

auto launcher_module::_draw_recent_projects() -> void {
  auto& projects_module = sbx::core::engine::get_module<sbx::core::projects_module>();

  const auto& recents = projects_module.recent_projects();

  if (recents.empty()) {
    ImGui::TextDisabled("No recent projects.");
    return;
  }

  // Deferred rather than called mid-loop: remove_recent() mutates the very vector `recents`
  // references, which would invalidate the loop underneath it.
  auto to_remove = std::optional<std::filesystem::path>{};

  if (ImGui::BeginTable("##recent_projects", 3, ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthFixed, 96.0f);

    for (const auto& recent : recents) {
      ImGui::PushID(recent.file.string().c_str());
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(recent.name.c_str());

      ImGui::TableSetColumnIndex(1);
      ImGui::TextDisabled("%s", recent.file.parent_path().string().c_str());

      ImGui::TableSetColumnIndex(2);

      if (ImGui::SmallButton(ICON_MDI_PENCIL)) {
        _launch(launch_target::editor, recent.file.parent_path());
      }

      ImGui::SameLine();

      if (ImGui::SmallButton(ICON_MDI_PLAY)) {
        _launch(launch_target::runtime, recent.file.parent_path());
      }

      ImGui::SameLine();

      if (ImGui::SmallButton(ICON_MDI_TRASH_CAN_OUTLINE)) {
        to_remove = recent.file;
      }

      ImGui::PopID();
    }

    ImGui::EndTable();
  }

  if (to_remove) {
    projects_module.remove_recent(*to_remove);
  }
}

auto launcher_module::_draw_new_project_name_dialog() -> void {
  if (_show_new_project_name_dialog) {
    ImGui::OpenPopup("New Project");
    _show_new_project_name_dialog = false;
    _new_project_name_buffer.fill('\0');
  }

  if (ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Location: %s", _new_project_parent.string().c_str());
    ImGui::InputText("Name", _new_project_name_buffer.data(), _new_project_name_buffer.size());

    const auto name = std::string{_new_project_name_buffer.data()};
    const auto can_create = !name.empty();

    ImGui::BeginDisabled(!can_create);

    if (ImGui::Button("Create")) {
      _launch(launch_target::editor, _new_project_parent / name);
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

auto launcher_module::_launch(launch_target target, const std::filesystem::path& root) -> void {
  auto& projects_module = sbx::core::engine::get_module<sbx::core::projects_module>();

  try {
    if (std::filesystem::exists(root / sbx::core::project::file_name)) {
      projects_module.open(root);
    } else {
      projects_module.create(root, root.filename().string());
    }
  } catch (const std::exception& exception) {
    sbx::utility::logger<"launcher">::error("Failed to open/create project at '{}': {}", root.string(), exception.what());
    return;
  }

  sbx::platform::spawn(executable_path(target), {.arguments = {"--project", (root / sbx::core::project::file_name).string()}});

  sbx::core::engine::quit();
}

} // namespace launcher
