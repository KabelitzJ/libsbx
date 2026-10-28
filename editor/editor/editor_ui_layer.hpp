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

#include <libsbx/signals/connection.hpp>

#include <editor/editor_state.hpp>
#include <editor/asset_watcher.hpp>
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

  /** @brief Opens @p path alongside the open scenes, like Unity's Open Scene Additive. No guard: nothing is replaced. */
  auto open_scene_additive(const std::filesystem::path& path) -> void;

  /** @brief Drops the undo history, whenever pushed commands can no longer be replayed (e.g. leaving play mode). */
  auto clear_command_stack() -> void {
    _state.clear_command_stack();
  }

private:

  enum class pending_scene_action { none, quit, new_scene, open_scene, unload_scene };

  auto _upload_fonts() -> void;

  auto _draw_dockspace() -> void;

  /** @brief Editor-wide shortcuts (Ctrl+Z/Y/C/V/D/S/N/O, Delete, Escape, F), skipped while a text field has focus. */
  auto _handle_shortcuts() -> void;

  /** @brief Saves the active scene, or Save As if it was never saved. */
  auto _save() -> void;

  /** @brief Saves every scene with unsaved changes; the first one never saved gets Save As. */
  auto _save_all() -> void;

  /**
   * @brief Saves one loaded scene to its own file.
   *
   * @return False when it was never saved and Save As opened instead.
   */
  auto _save_scene_or_prompt(std::uint32_t handle) -> bool;

  /** @brief Recomputes _state.dirty_scenes when the undo history moved or a scene was saved, loaded or unloaded. */
  auto _refresh_dirty_scenes() -> void;

  /** @brief The scenes an action would discard changes in: every loaded scene, or just @p handle for an unload. */
  [[nodiscard]] auto _dirty_scenes_for(pending_scene_action action, std::uint32_t handle) -> std::vector<std::uint32_t>;

  auto _unload_scene(std::uint32_t handle) -> void;

  /** @brief Consumes _state's scene save/unload requests (from the Hierarchy's scene headers). */
  auto _handle_scene_file_request() -> void;

  /** @brief The Play/Pause/Stop strip under the menu bar. */
  auto _draw_toolbar() -> void;

  auto _create_panels() -> void;

  auto _save_scene(std::uint32_t handle, const std::filesystem::path& path) -> void;

  /**
   * @brief Compares one loaded scene's serialized form with its file.
   *
   * @return True if there are unsaved changes, or the scene was never saved.
   */
  [[nodiscard]] auto _is_scene_dirty(std::uint32_t handle) -> bool;

  /** @brief Opens the Save As dialog for one loaded scene; any pending scene action runs once it resolves. */
  auto _open_save_as_dialog(std::uint32_t handle) -> void;

  /** @brief Polls the Save As dialog, saving on confirm and running a pending scene action if one was deferred. */
  auto _draw_save_as_dialog() -> void;

  // Opens Save As for the next never-saved scene a Save All still has to place, if any.
  auto _open_next_save_all_dialog() -> void;

  auto _draw_unsaved_changes_dialog() -> void;

  /**
   * @brief Runs @p action now if no scene it affects has unsaved changes, otherwise stashes it behind the unsaved-changes dialog.
   *
   * @param action The guarded action.
   * @param path The scene to open, for open actions.
   * @param handle The scene to unload, for unload actions.
   */
  auto _request_discard_confirmation(pending_scene_action action, std::filesystem::path path, std::uint32_t handle = 0u) -> void;

  /** @brief Runs and clears the pending scene action: quit, new scene or open _pending_open_path. */
  auto _run_pending_scene_action() -> void;

  auto _new_scene() -> void;

  auto _open_scene(const std::filesystem::path& path) -> void;

  auto _open_open_scene_dialog(bool additive = false) -> void;

  auto _draw_open_scene_dialog() -> void;

  // How the Viewport samples final_image.
  sbx::graphics::sampler _sampler;

  bool _viewport_is_hovered{false};

  editor_state _state{};

  asset_watcher _asset_watcher{};

  thumbnail_cache _thumbnails{};

  // Declared after _state, which the slot writes into, so it disconnects first.
  sbx::signals::scoped_connection _files_dropped{};
  std::vector<std::unique_ptr<editor_panel>> _panels{};

  // Owned by _panels; kept so the View menu can toggle it without a dynamic_cast.
  navigation_panel* _navigation_panel{nullptr};

  scene_renderer_panel* _scene_renderer_panel{nullptr};

  sbx::memory::observer_ptr<project_settings_panel> _project_settings_panel{};
  sbx::memory::observer_ptr<preferences_panel> _preferences_panel{};

  std::uint32_t _pending_scene_handle{0u}; // the scene a pending unload targets
  std::vector<std::uint32_t> _guarded_scenes{}; // the scenes the unsaved-changes dialog is about
  std::uint32_t _save_as_target{0u};
  std::vector<std::uint32_t> _save_all_queue{}; // never-saved scenes a Save All prompts for after the current Save As
  bool _open_dialog_additive{false};

  std::uint64_t _dirty_revision{0u};
  bool _dirty_scenes_stale{true};

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
