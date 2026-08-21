// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_LOGGER_PANEL_HPP_
#define EDITOR_PANELS_LOGGER_PANEL_HPP_

#include <array>
#include <cstddef>

#include <imgui.h>

#include <spdlog/common.h>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief Draws the Console panel: a live view of the engine's in-memory log ring buffer
 * (sbx::utility::logged_lines()), with per-level toggles, a text filter, and auto-scroll. Doesn't
 * use editor_state — the log stream isn't part of the shared selection/panel state, and no other
 * panel needs to read it.
 */
class logger_panel final : public editor_panel {

public:

  auto draw(editor_state& state) -> void override;

private:

  // One toggle per real level (trace..critical); "off" is never emitted, so it's excluded.
  static constexpr auto level_count = static_cast<std::size_t>(spdlog::level::n_levels) - 1u;

  ImGuiTextFilter _text_filter{};
  std::array<bool, level_count> _level_enabled{true, true, true, true, true, true};
  bool _is_auto_scroll{true};

}; // class logger_panel

} // namespace editor

#endif // EDITOR_PANELS_LOGGER_PANEL_HPP_
