// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_LOGGER_PANEL_HPP_
#define EDITOR_PANELS_LOGGER_PANEL_HPP_

namespace editor {

/**
 * @brief Draws the Console panel: a live view of the engine's in-memory log ring buffer
 * (sbx::utility::logged_lines()), with per-level toggles, a text filter, and auto-scroll.
 * Takes no editor_state — the log stream isn't part of the shared selection/panel state, and
 * no other panel needs to read it.
 */
auto draw_logger_panel() -> void;

} // namespace editor

#endif // EDITOR_PANELS_LOGGER_PANEL_HPP_
