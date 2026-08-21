// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_HIERARCHY_PANEL_HPP_
#define EDITOR_PANELS_HIERARCHY_PANEL_HPP_

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief The "Hierarchy" panel: a tree of the active scene's nodes. Clicking a row selects that
 * node in the shared editor_state; clicking empty space clears the selection.
 */
class hierarchy_panel final : public editor_panel {

public:

  auto draw(editor_state& state) -> void override;

}; // class hierarchy_panel

} // namespace editor

#endif // EDITOR_PANELS_HIERARCHY_PANEL_HPP_
