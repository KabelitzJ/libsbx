// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_
#define EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief Draws the "Asset Browser" panel: a two-pane view of the active project's assets
 * directory (folder tree left, current folder's contents right). Clicking an importable file
 * registers it with assets_module and selects it in @p state; clicking a folder navigates into it.
 */
auto draw_asset_browser_panel(editor_state& state) -> void;

} // namespace editor

#endif // EDITOR_PANELS_ASSET_BROWSER_PANEL_HPP_
