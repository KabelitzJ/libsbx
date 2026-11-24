// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_HISTORY_PANEL_HPP_
#define EDITOR_PANELS_HISTORY_PANEL_HPP_

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/** @brief Edit > History: the undo history as a list; clicking an entry undoes or redoes up to it, like Godot's History dock. */
class history_panel final : public editor_panel {

public:

  inline static constexpr auto window_name = ICON_MDI_HISTORY " History###history_panel";

  auto draw(editor_state& state) -> void override;

  bool is_open{false};

}; // class history_panel

} // namespace editor

#endif // EDITOR_PANELS_HISTORY_PANEL_HPP_
