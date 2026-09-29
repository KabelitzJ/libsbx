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

/**
 * @brief The editor's contribution to the engine's ImGui frame: dockspace, menu bar, the
 * Viewport panel (embeds scene_renderer_module::final_image via ImGui::Image()), the Stats
 * window, every registered editor_panel, and the scene save/quit dialogs.
 *
 * Registered with ui_module by editor_module, which owns this and stays a thin lifecycle
 * shell (see editor_module.hpp).
 */
class editor_ui_layer final : public sbx::utility::noncopyable, public sbx::render::ui_layer {

public:

  // Not a separate editor_panel (see panels/), so this window's constant lives here instead of on
  // a panel class — same "single source of truth for ImGui::Begin()'s exact string" reasoning as
  // hierarchy_panel::window_name and friends; referenced by both build() and _draw_dockspace().
  inline static constexpr auto viewport_window_name = ICON_MDI_GAMEPAD_VARIANT " Viewport###viewport_panel";

  editor_ui_layer();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Editor";
  }

  auto build() -> void override;

  /** @brief Whether the mouse was over the Viewport panel as of the last frame's UI pass. */
  [[nodiscard]] auto is_viewport_hovered() const noexcept -> bool {
    return _viewport_is_hovered;
  }

  /** @brief Called once by application.cpp right after its own initial scene load. */
  auto set_scene_path(std::filesystem::path path) -> void {
    _scene_path = std::move(path);
  }

  /**
   * @brief Quits immediately if the scene has no unsaved changes; otherwise shows a
   * confirmation dialog (Save / Don't Save / Cancel).
   *
   * Use instead of sbx::core::engine::quit() directly for anything that can originate outside
   * an explicit in-editor "discard everything" action (window close, File > Quit).
   */
  auto request_quit() -> void;

  /** @brief Same discard-guard as request_quit(), but replaces the active scene with a fresh blank one instead of quitting. */
  auto new_scene() -> void;

  /** @brief Same discard-guard as request_quit(), but loads path as the active scene instead of quitting. */
  auto open_scene(const std::filesystem::path& path) -> void;

  /** @brief Drops the undo/redo history — call whenever previously-pushed commands can no longer be safely replayed (see editor_module::exit_play_mode()). */
  auto clear_command_stack() -> void {
    _state.clear_command_stack();
  }

private:

  enum class pending_scene_action { none, quit, new_scene, open_scene };

  auto _upload_fonts() -> void;

  auto _draw_dockspace() -> void;

  /** @brief Editor-wide keyboard shortcuts (Ctrl+Z/Y/C/V/D/S/N/O, Delete, Escape, F); skipped while a text field has focus. */
  auto _handle_shortcuts() -> void;

  /** @brief Save to the current scene path, or Save As if the scene has never been saved. */
  auto _save() -> void;

  /** @brief The centered Play/Pause/Stop toolbar strip drawn directly under the main menu bar. */
  auto _draw_toolbar() -> void;

  auto _create_panels() -> void;

  auto _save_scene(const std::filesystem::path& path) -> void;

  /** @brief Compares the scene's current serialize() output against what's on disk at _scene_path. */
  [[nodiscard]] auto _is_scene_dirty() -> bool;

  /** @brief Opens _save_dialog seeded from the current _scene_path (or a fresh "new_scene.scene" if none yet). Whatever's pending in _pending_scene_action runs once the dialog resolves (see _run_pending_after_save_as). */
  auto _open_save_as_dialog() -> void;

  /** @brief Polls _save_dialog and, on a confirmed pick, saves there and — if _run_pending_after_save_as is set — runs the pending scene action. */
  auto _draw_save_as_dialog() -> void;

  auto _draw_unsaved_changes_dialog() -> void;

  /** @brief Shared by request_quit()/new_scene()/open_scene(): runs action immediately if the scene has no unsaved changes, otherwise stashes it and shows the unsaved-changes dialog. */
  auto _request_discard_confirmation(pending_scene_action action, std::filesystem::path path) -> void;

  /** @brief Runs (and clears) whatever's in _pending_scene_action — quit, a fresh blank scene, or loading _pending_open_path. */
  auto _run_pending_scene_action() -> void;

  auto _new_scene() -> void;

  auto _open_scene(const std::filesystem::path& path) -> void;

  auto _open_open_scene_dialog() -> void;

  auto _draw_open_scene_dialog() -> void;

  // Viewport panel's sampler for ImGui::Image()-sampling final_image — see
  // ui_module::texture_id(). Not a backend concern (that lives in ui_system), just how this one
  // image should be filtered.
  sbx::graphics::sampler _sampler;

  bool _viewport_is_hovered{false};

  editor_state _state{};
  std::vector<std::unique_ptr<editor_panel>> _panels{};

  // Non-owning -- _panels owns it. Kept separately so the View menu can toggle its is_open flag
  // without a dynamic_cast over every registered panel.
  navigation_panel* _navigation_panel{nullptr};

  // Same reasoning as _navigation_panel above.
  scene_renderer_panel* _scene_renderer_panel{nullptr};

  // Same reasoning as _navigation_panel above -- the Edit menu opens these.
  sbx::memory::observer_ptr<project_settings_panel> _project_settings_panel{};
  sbx::memory::observer_ptr<preferences_panel> _preferences_panel{};

  // Scene save/load path (relative to the assets directory) — empty until the first save, or
  // until application.cpp calls set_scene_path() after its own initial load.
  std::filesystem::path _scene_path{};

  sbx::render::file_dialog _save_dialog{};

  // Separate instance from _save_dialog -- both can't be mid-flight at once in practice, but each
  // owns its own imgui-file-dialog state and there's no reason to share it.
  sbx::render::file_dialog _open_dialog{};

  bool _show_unsaved_changes_dialog{false};

  // What request_quit()/new_scene()/open_scene() are guarding, and (for open_scene) the path to
  // load -- set by _request_discard_confirmation(), consumed by _run_pending_scene_action() once
  // either the scene turns out clean or the unsaved-changes dialog resolves.
  pending_scene_action _pending_scene_action{pending_scene_action::none};
  std::filesystem::path _pending_open_path{};

  // Set when the unsaved-changes dialog's "Save" has to detour through Save As (no _scene_path
  // yet) — consulted by _draw_save_as_dialog() so that detour still runs _pending_scene_action
  // once it completes, instead of silently dropping it.
  bool _run_pending_after_save_as{false};

}; // class editor_ui_layer

} // namespace editor

#endif // EDITOR_EDITOR_UI_LAYER_HPP_
