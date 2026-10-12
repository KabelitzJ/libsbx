// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/editor_ui_layer.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include <fmt/format.h>

#include <imgui.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/scene_serializer.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace editor {

auto editor_ui_layer::request_quit() -> void {
  _request_discard_confirmation(pending_scene_action::quit, {});
}

auto editor_ui_layer::new_scene() -> void {
  _request_discard_confirmation(pending_scene_action::new_scene, {});
}

auto editor_ui_layer::open_scene(const std::filesystem::path& path) -> void {
  _request_discard_confirmation(pending_scene_action::open_scene, path);
}

auto editor_ui_layer::open_scene_additive(const std::filesystem::path& path) -> void {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  // Like Unity, a scene is open at most once in the editor.
  const auto is_open = std::ranges::any_of(scenes_module.loaded_scenes(), [&](const sbx::scenes::loaded_scene& loaded) {
    return loaded.path.lexically_normal() == path.lexically_normal();
  });

  if (is_open) {
    sbx::utility::logger<"editor">::warn("Scene '{}' is already open", path.generic_string());
    return;
  }

  if (!assets_module.load_scene(path).is_valid()) {
    return;
  }

  scenes_module.load_scene(path, sbx::scenes::load_scene_mode::additive);
  scenes_module.apply_pending_operations();

  _dirty_scenes_stale = true;
}

auto editor_ui_layer::_request_discard_confirmation(pending_scene_action action, std::filesystem::path path, std::uint32_t handle) -> void {
  _pending_scene_action = action;
  _pending_open_path = std::move(path);
  _pending_scene_handle = handle;

  // Checked fresh rather than from _state.dirty_scenes, which only refreshes once per frame.
  _guarded_scenes = _dirty_scenes_for(action, handle);

  if (_guarded_scenes.empty()) {
    _run_pending_scene_action();
  } else {
    _show_unsaved_changes_dialog = true;
  }
}

auto editor_ui_layer::_dirty_scenes_for(pending_scene_action action, std::uint32_t handle) -> std::vector<std::uint32_t> {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto dirty = std::vector<std::uint32_t>{};

  for (const auto& loaded : scenes_module.loaded_scenes()) {
    if (loaded.handle == sbx::scenes::persistent_scene_handle) {
      continue;
    }

    if (action == pending_scene_action::unload_scene && loaded.handle != handle) {
      continue;
    }

    if (_is_scene_dirty(loaded.handle)) {
      dirty.push_back(loaded.handle);
    }
  }

  return dirty;
}

auto editor_ui_layer::_run_pending_scene_action() -> void {
  const auto action = std::exchange(_pending_scene_action, pending_scene_action::none);
  const auto path = std::exchange(_pending_open_path, std::filesystem::path{});

  switch (action) {
    case pending_scene_action::none: {
      break;
    }
    case pending_scene_action::quit: {
      sbx::core::engine::quit();
      break;
    }
    case pending_scene_action::new_scene: {
      _new_scene();
      break;
    }
    case pending_scene_action::open_scene: {
      _open_scene(path);
      break;
    }
    case pending_scene_action::unload_scene: {
      _unload_scene(_pending_scene_handle);
      break;
    }
  }
}

auto editor_ui_layer::_save_scene(std::uint32_t handle, const std::filesystem::path& path) -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (!scenes_module.find_loaded_scene(handle)) {
    return;
  }

  auto asset = assets_module.create_scene(sbx::scenes::scene_serializer::build(scenes_module.active_scene(), handle), path.stem().string());
  assets_module.save_scene(asset, path);

  scenes_module.set_scene_path(handle, path);

  _dirty_scenes_stale = true;
}

auto editor_ui_layer::_save_scene_or_prompt(std::uint32_t handle) -> bool {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  const auto loaded = scenes_module.find_loaded_scene(handle);

  if (!loaded || loaded->handle == sbx::scenes::persistent_scene_handle) {
    return true;
  }

  if (loaded->path.empty()) {
    _open_save_as_dialog(handle);
    return false;
  }

  _save_scene(handle, loaded->path);

  return true;
}

auto editor_ui_layer::_save() -> void {
  _save_scene_or_prompt(sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene_handle());
}

auto editor_ui_layer::_save_all() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto never_saved = std::optional<std::uint32_t>{};

  for (const auto& loaded : scenes_module.loaded_scenes()) {
    if (loaded.handle == sbx::scenes::persistent_scene_handle || !_is_scene_dirty(loaded.handle)) {
      continue;
    }

    if (loaded.path.empty()) {
      never_saved = never_saved.value_or(loaded.handle);
    } else {
      _save_scene(loaded.handle, loaded.path);
    }
  }

  // ponytail: one Save As per Save All; a second never-saved scene needs another Save All.
  if (never_saved) {
    _open_save_as_dialog(*never_saved);
  }
}

auto editor_ui_layer::_is_scene_dirty(std::uint32_t handle) -> bool {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  const auto loaded = scenes_module.find_loaded_scene(handle);

  if (!loaded || loaded->path.empty()) {
    return true; // never saved
  }

  auto file = std::ifstream{sbx::core::engine::project().assets_directory() / loaded->path, std::ios::binary};

  if (!file) {
    return true; // no file there yet
  }

  const auto on_disk = std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};

  return on_disk != sbx::scenes::scene_serializer::serialize(scenes_module.active_scene(), handle);
}

auto editor_ui_layer::_refresh_dirty_scenes() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  // Play-mode edits are thrown away on Stop, so they never count as unsaved.
  if (scenes_module.is_simulating()) {
    _state.dirty_scenes.clear();
    _dirty_scenes_stale = true;
    return;
  }

  if (!_dirty_scenes_stale && _state.commands.revision() == _dirty_revision) {
    return;
  }

  _state.dirty_scenes.clear();

  for (const auto& loaded : scenes_module.loaded_scenes()) {
    if (loaded.handle != sbx::scenes::persistent_scene_handle && _is_scene_dirty(loaded.handle)) {
      _state.dirty_scenes.push_back(loaded.handle);
    }
  }

  _dirty_revision = _state.commands.revision();
  _dirty_scenes_stale = false;
}

auto editor_ui_layer::_unload_scene(std::uint32_t handle) -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  scenes_module.unload_scene(handle);
  scenes_module.apply_pending_operations();

  // Undo steps may refer to the unloaded scene's nodes.
  _state.clear_selection();
  _state.clear_command_stack();

  _dirty_scenes_stale = true;
}

auto editor_ui_layer::_handle_scene_file_request() -> void {
  if (!_state.pending_scene_file_request) {
    return;
  }

  const auto request = *std::exchange(_state.pending_scene_file_request, std::nullopt);

  switch (request.action) {
    case editor_state::scene_file_action::save: {
      _save_scene_or_prompt(request.handle);
      break;
    }
    case editor_state::scene_file_action::save_as: {
      _open_save_as_dialog(request.handle);
      break;
    }
    case editor_state::scene_file_action::unload: {
      _request_discard_confirmation(pending_scene_action::unload_scene, {}, request.handle);
      break;
    }
  }
}

auto editor_ui_layer::_open_save_as_dialog(std::uint32_t handle) -> void {
  auto& project = sbx::core::engine::project();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  const auto loaded = scenes_module.find_loaded_scene(handle);
  const auto current_path = loaded ? loaded->path : std::filesystem::path{};
  const auto name = loaded ? loaded->name : std::string{"new_scene"};

  _save_as_target = handle;

  const auto start_dir = current_path.empty() ? project.assets_directory() : (project.assets_directory() / current_path).parent_path();
  const auto default_name = current_path.empty() ? fmt::format("{}.scene", name) : current_path.filename().string();

  _save_dialog.open({
    .title = fmt::format("Save Scene '{}' As", name),
    .mode = sbx::render::file_dialog_mode::save_file,
    .start_dir = start_dir,
    .extensions = {".scene"},
    .shortcuts = {{.label = "Assets", .path = project.assets_directory()}},
    .default_file_name = default_name,
  });
}

auto editor_ui_layer::_draw_save_as_dialog() -> void {
  _save_dialog.draw();

  auto picked = _save_dialog.result();

  if (!picked) {
    return;
  }

  if (picked->empty()) {
    // Cancelled: don't run the pending action on some unrelated later save.
    _run_pending_after_save_as = false;
    return;
  }

  auto& project = sbx::core::engine::project();

  auto ec = std::error_code{};
  const auto relative = std::filesystem::relative(picked->front(), project.assets_directory(), ec);

  // Relative to the assets directory when the pick is inside it, otherwise absolute; save_scene accepts both.
  const auto scene_path = (!ec && !relative.empty() && relative.begin()->string() != "..") ? relative : picked->front();

  _save_scene(_save_as_target, scene_path);

  if (_run_pending_after_save_as) {
    _run_pending_after_save_as = false;

    // Guarded again rather than run: another never-saved scene may still need its own Save As.
    _request_discard_confirmation(_pending_scene_action, _pending_open_path, _pending_scene_handle);
  }
}

auto editor_ui_layer::_draw_unsaved_changes_dialog() -> void {
  if (_show_unsaved_changes_dialog) {
    ImGui::OpenPopup("Unsaved Changes");
    _show_unsaved_changes_dialog = false;
  }

  if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    ImGui::Text(ICON_MDI_CONTENT_SAVE_ALERT " These scenes have unsaved changes:");

    for (const auto handle : _guarded_scenes) {
      if (const auto loaded = scenes_module.find_loaded_scene(handle)) {
        ImGui::BulletText("%s", loaded->name.c_str());
      }
    }

    if (ImGui::Button(ICON_MDI_CONTENT_SAVE " Save")) {
      auto all_saved = true;

      for (const auto handle : _guarded_scenes) {
        if (!_save_scene_or_prompt(handle)) {
          // Save As opened; the action is guarded again once it's done.
          _run_pending_after_save_as = true;
          all_saved = false;
          break;
        }
      }

      if (all_saved) {
        _run_pending_scene_action();
      }

      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Don't Save")) {
      ImGui::CloseCurrentPopup();
      _run_pending_scene_action();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
      _pending_scene_action = pending_scene_action::none;
    }

    ImGui::EndPopup();
  }
}

auto editor_ui_layer::_new_scene() -> void {
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  scenes_module.new_scene();
  scenes_module.apply_pending_operations();

  // The same default content as application.cpp's startup fallback: a play camera with a sky.
  auto camera = scenes_module.active_scene().active_camera();

  auto& skybox = camera.add_component<sbx::scenes::skybox>();
  skybox.environment = assets_module.load_environment_map("environments/sky.hdr");
  skybox.intensity = 1.0f;

  _state.clear_selection();
  _state.clear_command_stack();

  _dirty_scenes_stale = true;
}

auto editor_ui_layer::_open_scene(const std::filesystem::path& path) -> void {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  if (!assets_module.load_scene(path).is_valid()) {
    return;
  }

  scenes_module.load_scene(path);
  scenes_module.apply_pending_operations();

  _state.clear_selection();
  _state.clear_command_stack();

  _dirty_scenes_stale = true;
}

auto editor_ui_layer::_open_open_scene_dialog(bool additive) -> void {
  auto& project = sbx::core::engine::project();

  _open_dialog_additive = additive;

  _open_dialog.open({
    .title = additive ? "Open Scene Additive" : "Open Scene",
    .mode = sbx::render::file_dialog_mode::open_file,
    .start_dir = project.assets_directory(),
    .extensions = {".scene"},
    .shortcuts = {{.label = "Assets", .path = project.assets_directory()}},
  });
}

auto editor_ui_layer::_draw_open_scene_dialog() -> void {
  _open_dialog.draw();

  auto picked = _open_dialog.result();

  if (!picked || picked->empty()) {
    return;
  }

  auto& project = sbx::core::engine::project();

  auto ec = std::error_code{};
  const auto relative = std::filesystem::relative(picked->front(), project.assets_directory(), ec);

  const auto scene_path = (!ec && !relative.empty() && relative.begin()->string() != "..") ? relative : picked->front();

  if (_open_dialog_additive) {
    open_scene_additive(scene_path);
  } else {
    open_scene(scene_path); // re-enters the unsaved-changes guard in case the scene changed meanwhile
  }
}

} // namespace editor
