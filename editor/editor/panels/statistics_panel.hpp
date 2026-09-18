// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_STATISTICS_PANEL_HPP_
#define EDITOR_PANELS_STATISTICS_PANEL_HPP_

#include <cstddef>

#include <libsbx/math/smooth_value.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief Tracy-independent rendering/performance/memory stats -- Renderer/Performance/Memory
 * tabs. Always docked (see editor_ui_layer's default layout), same as the inline FPS-only window
 * this replaces.
 *
 * Everything shown is read fresh every draw() call from whichever module already owns it (pull,
 * not push) -- see scene_renderer_panel for the render-graph's own GPU timing/pipeline stats.
 */
class statistics_panel final : public editor_panel {

public:

  inline static constexpr auto window_name = ICON_MDI_CHART_BAR " Statistics###statistics_panel";

  auto draw(editor_state& state) -> void override;

private:

  auto _draw_renderer_tab() -> void;

  auto _draw_performance_tab() -> void;

  auto _draw_memory_tab() -> void;

  sbx::math::proportional_smooth_value _smoothed_frame_time_ms{0.0f};

  // Previous draw() call's snapshot -- _draw_memory_tab() shows the delta since then (this
  // frame's allocation activity) rather than the ever-growing lifetime totals, which said
  // nothing at a glance beyond "this number only goes up".
  std::size_t _prev_total_allocated{0u};
  std::size_t _prev_total_freed{0u};
  std::size_t _prev_alloc_count{0u};
  std::size_t _prev_dealloc_count{0u};

}; // class statistics_panel

} // namespace editor

#endif // EDITOR_PANELS_STATISTICS_PANEL_HPP_
