// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_PROPERTIES_PANEL_HPP_
#define EDITOR_PANELS_PROPERTIES_PANEL_HPP_

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws the "Properties" panel: an inspector for whatever @p state's current selection is
 * — a scene node's name/transform/components, a read-only asset summary, or an empty-state
 * message if nothing is selected.
 */
auto draw_properties_panel(editor_state& state) -> void;

} // namespace editor

#endif // EDITOR_PANELS_PROPERTIES_PANEL_HPP_
