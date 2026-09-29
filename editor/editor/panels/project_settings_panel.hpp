// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_PROJECT_SETTINGS_PANEL_HPP_
#define EDITOR_PANELS_PROJECT_SETTINGS_PANEL_HPP_

#include <cstdint>
#include <vector>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief Edit > Project Settings: the project-wide settings stored in project.sbxproj (General, Physics, Layers). Every edit
 * saves the project file right away -- no Apply step, no undo (project settings aren't part of the scene's command history).
 * The Layers tab also opens on its own from any layer dropdown's "Edit Layers..." (editor_state::request_open_layer_settings).
 */
class project_settings_panel final : public editor_panel {

public:

  inline static constexpr auto window_name = ICON_MDI_COG " Project Settings###project_settings_panel";

  auto draw(editor_state& state) -> void override;

  bool is_open{false};

private:

  auto _draw_general_tab() -> void;

  auto _draw_physics_tab() -> void;

  auto _draw_layers_tab() -> void;

  // Which layer indices the Layers tab shows a row for, in display order -- resynced from core::project::layers() whenever
  // the tab (re)appears, then changed only by its own Add (appended at the bottom) and each row's "x" (erased). Deliberately
  // not recomputed every frame: renaming a layer through a transient empty string while retyping must not make its row vanish.
  std::vector<std::uint8_t> _layer_rows{};
  bool _layers_tab_was_visible{false};

}; // class project_settings_panel

} // namespace editor

#endif // EDITOR_PANELS_PROJECT_SETTINGS_PANEL_HPP_
