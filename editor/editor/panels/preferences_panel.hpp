// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_PREFERENCES_PANEL_HPP_
#define EDITOR_PANELS_PREFERENCES_PANEL_HPP_

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/** @brief Edit > Preferences: per-user editor settings (editor_preferences) plus the editor camera's own speed/sensitivity/lens. */
class preferences_panel final : public editor_panel {

public:

  inline static constexpr auto window_name = ICON_MDI_TUNE " Preferences###preferences_panel";

  auto draw(editor_state& state) -> void override;

  bool is_open{false};

}; // class preferences_panel

} // namespace editor

#endif // EDITOR_PANELS_PREFERENCES_PANEL_HPP_
