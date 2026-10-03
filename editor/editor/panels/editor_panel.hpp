// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_EDITOR_PANEL_HPP_
#define EDITOR_PANELS_EDITOR_PANEL_HPP_

#include <editor/editor_state.hpp>

namespace editor {

/** @brief Base for a top-level editor window; panels own their state and share only editor_state. */
class editor_panel {

public:

  virtual ~editor_panel() = default;

  /**
   * @brief Draws the panel's window for this frame.
   *
   * @param state The editor state.
   */
  virtual auto draw(editor_state& state) -> void = 0;

}; // class editor_panel

} // namespace editor

#endif // EDITOR_PANELS_EDITOR_PANEL_HPP_
