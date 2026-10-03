// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_PROJECT_SETTINGS_PANEL_HPP_
#define EDITOR_PANELS_PROJECT_SETTINGS_PANEL_HPP_

#include <cstdint>
#include <vector>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/** @brief Edit > Project Settings (General, Physics, Layers) from project.sbxproj; every edit saves immediately, with no undo. Layer fields' "Edit Layers..." opens the Layers tab. */
class project_settings_panel final : public editor_panel {

public:

  inline static constexpr auto window_name = ICON_MDI_COG " Project Settings###project_settings_panel";

  auto draw(editor_state& state) -> void override;

  bool is_open{false};

private:

  auto _draw_general_tab() -> void;

  auto _draw_physics_tab() -> void;

  auto _draw_layers_tab() -> void;

  // The layer rows in display order, resynced when the tab appears and changed only by Add and each row's remove, so a layer being retyped through an empty name doesn't vanish.
  std::vector<std::uint8_t> _layer_rows{};
  bool _layers_tab_was_visible{false};

}; // class project_settings_panel

} // namespace editor

#endif // EDITOR_PANELS_PROJECT_SETTINGS_PANEL_HPP_
