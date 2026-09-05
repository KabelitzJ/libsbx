// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_UI_LAYER_HPP_
#define EDITOR_EDITOR_UI_LAYER_HPP_

#include <array>
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

#include <editor/editor_state.hpp>
#include <editor/panels/editor_panel.hpp>

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

  // Not a separate editor_panel (see panels/), so these two windows' constants live here instead
  // of on a panel class — same "single source of truth for ImGui::Begin()'s exact string" reasoning
  // as hierarchy_panel::window_name and friends; referenced by both build() and _draw_dockspace().
  inline static constexpr auto viewport_window_name = ICON_MDI_GAMEPAD_VARIANT " Viewport###viewport_panel";
  inline static constexpr auto stats_window_name = ICON_MDI_CHART_BAR " Statistics###statistics_panel";

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

  /** @brief Drops the undo/redo history — call whenever previously-pushed commands can no longer be safely replayed (see editor_module::exit_play_mode()). */
  auto clear_command_stack() -> void {
    _state.clear_command_stack();
  }

private:

  auto _upload_fonts() -> void;

  auto _draw_dockspace() -> void;

  /** @brief The centered Play/Pause/Stop toolbar strip drawn directly under the main menu bar. */
  auto _draw_toolbar() -> void;

  auto _create_panels() -> void;

  auto _save_scene(const std::filesystem::path& path) -> void;

  /** @brief Compares the scene's current serialize() output against what's on disk at _scene_path. */
  [[nodiscard]] auto _is_scene_dirty() -> bool;

  auto _draw_save_as_dialog() -> void;

  auto _draw_unsaved_changes_dialog() -> void;

  // Viewport panel's sampler for ImGui::Image()-sampling final_image — see
  // ui_module::texture_id(). Not a backend concern (that lives in ui_system), just how this one
  // image should be filtered.
  sbx::graphics::sampler _sampler;

  bool _viewport_is_hovered{false};

  editor_state _state{};
  std::vector<std::unique_ptr<editor_panel>> _panels{};

  // Scene save/load path (relative to the assets directory) — empty until the first save, or
  // until application.cpp calls set_scene_path() after its own initial load.
  std::filesystem::path _scene_path{};

  bool _show_save_as_dialog{false};
  std::array<char, 256u> _save_as_buffer{};

  bool _show_unsaved_changes_dialog{false};

  // Set when the unsaved-changes dialog's "Save" has to detour through Save As (no _scene_path
  // yet) — consulted by _draw_save_as_dialog() so that detour still quits once it completes,
  // instead of silently dropping the original quit request.
  bool _quit_after_save_as{false};

}; // class editor_ui_layer

} // namespace editor

#endif // EDITOR_EDITOR_UI_LAYER_HPP_
