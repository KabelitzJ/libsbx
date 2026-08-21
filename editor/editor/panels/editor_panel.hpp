// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_EDITOR_PANEL_HPP_
#define EDITOR_PANELS_EDITOR_PANEL_HPP_

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Base for a top-level editor window. Each concrete panel is instantiated once by
 * editor_module and owns whatever per-panel state it needs (filters, edit caches, ...) as
 * ordinary members instead of function-local statics. Panels never reference each other directly
 * — the only thing they share is editor_state, and only for genuinely cross-panel concerns
 * (selection, the viewport gizmo's operation/mode).
 */
class editor_panel {

public:

  virtual ~editor_panel() = default;

  /** @brief Draws this panel's ImGui::Begin/End window for the current frame. */
  virtual auto draw(editor_state& state) -> void = 0;

}; // class editor_panel

} // namespace editor

#endif // EDITOR_PANELS_EDITOR_PANEL_HPP_
