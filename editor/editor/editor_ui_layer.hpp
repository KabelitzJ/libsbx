// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_UI_LAYER_HPP_
#define EDITOR_EDITOR_UI_LAYER_HPP_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/resources/sampler.hpp>

#include <libsbx/render/ui/ui_layer.hpp>
#include <libsbx/render/ui/fonts/material_design_icons.hpp>
#include <libsbx/render/ui/widgets/file_dialog.hpp>

#include <editor/editor_state.hpp>
#include <editor/panels/editor_panel.hpp>
#include <libsbx/memory/observer_ptr.hpp>

#include <editor/panels/navigation_panel.hpp>
#include <editor/panels/preferences_panel.hpp>
#include <editor/panels/project_settings_panel.hpp>
#include <editor/panels/scene_renderer_panel.hpp>

namespace editor {

/** @brief The editor's ImGui layer: dockspace, menu bar, Viewport, every editor_panel and the scene save/quit dialogs. Owned and registered by editor_module. */
class editor_ui_layer final : public sbx::utility::noncopyable, public sbx::render::ui_layer {

public:

  // The Viewport window's ImGui::Begin() string, shared by build() and the dock layout.
  inline static constexpr auto viewport_window_name = ICON_MDI_GAMEPAD_VARIANT " Viewport###viewport_panel";

  editor_ui_layer();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Editor";
  }

  auto build() -> void override;

  /**
   * @brief Whether the mouse was over the Viewport as of the last UI pass.
   *
   * @return True if hovered.
   */
  [[nodiscard]] auto is_viewport_hovered() const noexcept -> bool {
    return _viewport_is_hovered;
  }

  /**
   * @brief Sets the current scene's path after application.cpp's initial load.
   *
   * @param path The scene path, relative to the assets directory.
   */
  auto set_scene_path(std::filesystem::path path) -> void {
    _scene_path = std::move(path);
  }

  /** @brief Quits, asking Save / Don't Save / Cancel first if the scene has unsaved changes. Use instead of engine::quit() for window close and File > Quit. */
  auto request_quit() -> void;

  /** @brief Replaces the scene with a blank one, with the same unsaved-changes guard as request_quit(). */
  auto new_scene() -> void;

  /**
   * @brief Loads @p path as the active scene, with the same unsaved-changes guard as request_quit().
   *
   * @param path The scene to open.
   */
  auto open_scene(const std::filesystem::path& path) -> void;

  /** @brief Drops the undo history, whenever pushed commands can no longer be replayed (e.g. leaving play mode). */
  auto clear_command_stack() -> void {
    _state.clear_command_stack();
  }

private:

  enum class pending_scene_action { none, quit, new_scene, open_scene };

  auto _upload_fonts() -> void;

  auto _draw_dockspace() -> void;

  /** @brief Editor-wide shortcuts (Ctrl+Z/Y/C/V/D/S/N/O, Delete, Escape, F), skipped while a text field has focus. */
  auto _handle_shortcuts() -> void;

  /** @brief Saves to the current scene path, or Save As if the scene was never saved. */
  auto _save() -> void;

  /** @brief The Play/Pause/Stop strip under the menu bar. */
  auto _draw_toolbar() -> void;

  auto _create_panels() -> void;

  auto _save_scene(const std::filesystem::path& path) -> void;

  /**
   * @brief Compares the scene's serialized form with the file at _scene_path.
   *
   * @return True if there are unsaved changes.
   */
  [[nodiscard]] auto _is_scene_dirty() -> bool;

  /** @brief Opens the Save As dialog; any pending scene action runs once it resolves. */
  auto _open_save_as_dialog() -> void;

  /** @brief Polls the Save As dialog, saving on confirm and running a pending scene action if one was deferred. */
  auto _draw_save_as_dialog() -> void;

  auto _draw_unsaved_changes_dialog() -> void;

  /**
   * @brief Runs @p action now if the scene is clean, otherwise stashes it behind the unsaved-changes dialog.
   *
   * @param action The guarded action.
   * @param path The scene to open, for open actions.
   */
  auto _request_discard_confirmation(pending_scene_action action, std::filesystem::path path) -> void;

  /** @brief Runs and clears the pending scene action: quit, new scene or open _pending_open_path. */
  auto _run_pending_scene_action() -> void;

  auto _new_scene() -> void;

  auto _open_scene(const std::filesystem::path& path) -> void;

  auto _open_open_scene_dialog() -> void;

  auto _draw_open_scene_dialog() -> void;

  // How the Viewport samples final_image.
  sbx::graphics::sampler _sampler;

  bool _viewport_is_hovered{false};

  editor_state _state{};
  std::vector<std::unique_ptr<editor_panel>> _panels{};

  // Owned by _panels; kept so the View menu can toggle it without a dynamic_cast.
  navigation_panel* _navigation_panel{nullptr};

  scene_renderer_panel* _scene_renderer_panel{nullptr};

  sbx::memory::observer_ptr<project_settings_panel> _project_settings_panel{};
  sbx::memory::observer_ptr<preferences_panel> _preferences_panel{};

  // Relative to the assets directory; empty until the first save or set_scene_path().
  std::filesystem::path _scene_path{};

  sbx::render::file_dialog _save_dialog{};

  sbx::render::file_dialog _open_dialog{};

  bool _show_unsaved_changes_dialog{false};

  // The action the unsaved-changes guard is holding, run by _run_pending_scene_action().
  pending_scene_action _pending_scene_action{pending_scene_action::none};
  std::filesystem::path _pending_open_path{};

  // Set when the dialog's Save must detour through Save As, so the pending action still runs afterwards.
  bool _run_pending_after_save_as{false};

}; // class editor_ui_layer

} // namespace editor

#endif // EDITOR_EDITOR_UI_LAYER_HPP_
