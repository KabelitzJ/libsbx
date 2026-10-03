// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_NAVIGATION_PANEL_HPP_
#define EDITOR_PANELS_NAVIGATION_PANEL_HPP_

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <libsbx/physics/nav/nav_settings.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/** @brief Edits physics::nav_settings and bakes the navmesh in Edit mode. Opened from the View menu. */
class navigation_panel final : public editor_panel {

public:

  inline static constexpr auto window_name = ICON_MDI_MAP_MARKER_PATH " Navigation###navigation_panel";

  auto draw(editor_state& state) -> void override;

  bool is_open{false};

private:

  sbx::physics::nav_settings _settings{};

}; // class navigation_panel

} // namespace editor

#endif // EDITOR_PANELS_NAVIGATION_PANEL_HPP_
