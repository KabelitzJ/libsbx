// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_EDITOR_PANEL_HPP_
#define EDITOR_PANELS_EDITOR_PANEL_HPP_

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Base for a top-level editor window.
 *
 * Each concrete panel is instantiated once by editor_module and owns its per-panel state
 * (filters, edit caches, ...) as ordinary members. Panels never reference each other directly —
 * the only shared state is editor_state, for cross-panel concerns like selection.
 */
class editor_panel {

public:

  virtual ~editor_panel() = default;

  /** @brief Draws this panel's ImGui::Begin/End window for the current frame. */
  virtual auto draw(editor_state& state) -> void = 0;

}; // class editor_panel

} // namespace editor

#endif // EDITOR_PANELS_EDITOR_PANEL_HPP_
