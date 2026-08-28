// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <launcher/launcher_module.hpp>

#include <stdexcept>
#include <string>
#include <utility>

#include <imgui.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/target.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/window.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#if defined(SBX_PLATFORM_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#else
  #include <unistd.h>
#endif

namespace launcher {

namespace {

#if defined(SBX_PLATFORM_WIN32)

auto spawn_editor(const std::filesystem::path& editor_executable, const std::filesystem::path& project_root) -> void {
  auto command_line = std::wstring{L"\""} + editor_executable.wstring() + L"\" --project \"" + project_root.wstring() + L"\"";

  auto startup_info = STARTUPINFOW{};
  startup_info.cb = sizeof(startup_info);

  auto process_info = PROCESS_INFORMATION{};

  const auto succeeded = ::CreateProcessW(editor_executable.c_str(), command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup_info, &process_info);

  if (!succeeded) {
    sbx::utility::logger<"launcher">::error("Failed to launch '{}'", editor_executable.string());
    return;
  }

  ::CloseHandle(process_info.hProcess);
  ::CloseHandle(process_info.hThread);
}

#else

auto spawn_editor(const std::filesystem::path& editor_executable, const std::filesystem::path& project_root) -> void {
  const auto pid = ::fork();

  if (pid < 0) {
    sbx::utility::logger<"launcher">::error("fork() failed while launching '{}'", editor_executable.string());
    return;
  }

  if (pid == 0) {
    // Child: replace this process image entirely. execl only returns on failure.
    ::execl(editor_executable.c_str(), editor_executable.c_str(), "--project", project_root.c_str(), static_cast<char*>(nullptr));
    ::_exit(127);
  }
}

#endif

/** @brief Resolved next to the launcher's own binary — both share RUNTIME_OUTPUT_DIRECTORY (see launcher/CMakeLists.txt / editor/CMakeLists.txt). */
[[nodiscard]] auto editor_executable_path() -> std::filesystem::path {
#if defined(SBX_PLATFORM_WIN32)
  return sbx::filesystem::executable_directory() / "editor.exe";
#else
  return sbx::filesystem::executable_directory() / "editor";
#endif
}

} // namespace

launcher_module::launcher_module() {
  auto& platform_module = sbx::core::engine::get_module<sbx::platform::platform_module>();
  auto& window = platform_module.window();

  window.on_window_closed() += [](const auto&) {
    sbx::core::engine::quit();
  };

  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();
  render_module.ui().add_default_fonts(16.0f);
  render_module.ui().add_layer(this);
}

launcher_module::~launcher_module() {
  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();
  render_module.ui().remove_layer(this);
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
    _file_dialog.open("Open Project", sbx::render::widgets::file_dialog_mode::select_folder);
  }

  ImGui::Separator();

  _draw_recent_projects();

  ImGui::End();

  _file_dialog.draw();

  if (auto picked = _file_dialog.result(); picked && !picked->empty()) {
    const auto folder = picked->front();
    const auto pending = std::exchange(_pending_pick, pending_pick::none);

    if (pending == pending_pick::open_project) {
      if (std::filesystem::exists(folder / sbx::core::project::file_name)) {
        _launch_editor(folder);
      } else {
        // No project.sbxproj here — fall through to New Project for this folder, same as §3
        // of the project plan calls for.
        _new_project_parent = folder;
        _show_new_project_name_dialog = true;
      }
    } else if (pending == pending_pick::new_project_parent) {
      _new_project_parent = folder;
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

  for (const auto& recent : recents) {
    ImGui::PushID(recent.file.string().c_str());

    if (ImGui::Selectable(recent.name.c_str())) {
      _launch_editor(recent.file.parent_path());
    }

    ImGui::TextDisabled("%s", recent.file.string().c_str());

    ImGui::PopID();
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
      _launch_editor(_new_project_parent / name);
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

auto launcher_module::_launch_editor(const std::filesystem::path& root) -> void {
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

  spawn_editor(editor_executable_path(), root);

  sbx::core::engine::quit();
}

} // namespace launcher
