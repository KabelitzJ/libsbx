// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_STATISTICS_PANEL_HPP_
#define EDITOR_PANELS_STATISTICS_PANEL_HPP_

#include <cstddef>
#include <vector>

#include <libsbx/math/smooth_value.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/** @brief Renderer, Performance and Memory stats, independent of Tracy and read fresh each draw from their owning modules. */
class statistics_panel final : public editor_panel {

public:

  inline static constexpr auto window_name = ICON_MDI_CHART_BAR " Statistics###statistics_panel";

  auto draw(editor_state& state) -> void override;

private:

  auto _draw_renderer_tab() -> void;

  auto _draw_performance_tab() -> void;

  auto _draw_memory_tab() -> void;

  /** @brief Samples memory every memory_sample_interval and drops samples older than memory_history_seconds; runs every draw so the graph has no gaps. */
  auto _sample_memory() -> void;

  auto _draw_memory_graph() -> void;

  inline static constexpr auto memory_sample_interval = 0.1f; // seconds
  inline static constexpr auto memory_history_seconds = 60.0f;

  sbx::math::proportional_smooth_value _smoothed_frame_time_ms{0.0f};

  // The previous draw's totals, so the Memory tab shows this frame's activity instead of ever-growing lifetime totals.
  std::size_t _prev_total_allocated{0u};
  std::size_t _prev_total_freed{0u};
  std::size_t _prev_alloc_count{0u};
  std::size_t _prev_dealloc_count{0u};

  // Parallel arrays for ImPlot: sample time and usage in MB.
  std::vector<float> _memory_times{};
  std::vector<float> _process_memory_mb{};
  std::vector<float> _tracked_memory_mb{};

}; // class statistics_panel

} // namespace editor

#endif // EDITOR_PANELS_STATISTICS_PANEL_HPP_
