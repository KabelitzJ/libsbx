// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_LOGGER_PANEL_HPP_
#define EDITOR_PANELS_LOGGER_PANEL_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <imgui.h>

#include <spdlog/common.h>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/console_sink.hpp>
#include <editor/panels/editor_panel.hpp>

namespace editor {

/** @brief The Console: the console sink's recent lines with per-level toggles, a text filter and auto-scroll. */
class logger_panel final : public editor_panel {

public:

  // The panel's ImGui::Begin() string.
  inline static constexpr auto window_name = ICON_MDI_CONSOLE " Console###logger_panel";

  auto draw(editor_state& state) -> void override;

private:

  // One toggle per emitted level; "off" is never emitted.
  static constexpr auto level_count = static_cast<std::size_t>(spdlog::level::n_levels) - 1u;

  // One display row: a line of a log entry (entries can span several, e.g. compiler diagnostics), so every row has the same height for ImGuiListClipper.
  struct row {
    spdlog::level::level_enum level{spdlog::level::info};
    std::string_view text{};
  }; // struct row

  // Rebuilds _rows from _lines with the current level toggles and text filter.
  auto _rebuild_rows() -> void;

  ImGuiTextFilter _text_filter{};
  std::array<bool, level_count> _level_enabled{true, true, true, true, true, true};
  bool _is_auto_scroll{true};

  // Copied from the sink only when its revision moves; _rows views into these strings.
  std::vector<log_line> _lines{};
  std::vector<row> _rows{};
  std::uint64_t _seen_revision{0u};
  bool _is_first_draw{true};

}; // class logger_panel

} // namespace editor

#endif // EDITOR_PANELS_LOGGER_PANEL_HPP_
