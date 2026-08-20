// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_HIERARCHY_PANEL_HPP_
#define EDITOR_PANELS_HIERARCHY_PANEL_HPP_

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws the "Hierarchy" panel: a tree of the active scene's nodes. Clicking a row selects
 * that node in @p state; clicking empty space clears the selection.
 */
auto draw_hierarchy_panel(editor_state& state) -> void;

} // namespace editor

#endif // EDITOR_PANELS_HIERARCHY_PANEL_HPP_
