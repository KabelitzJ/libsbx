// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_SCENE_RENDERER_PANEL_HPP_
#define EDITOR_PANELS_SCENE_RENDERER_PANEL_HPP_

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief Per-pass GPU time and whole-frame pipeline statistics for the active scene's
 * render_graph -- see render_graph::pass_timings()/pipeline_stats(). On-demand (Window menu),
 * same pattern as navigation_panel -- see its own doc comment on why.
 *
 * Every number here is read back from a VkQueryPool and is therefore always
 * swapchain::max_frames_in_flight frames stale, same as Tracy's own GPU zones would be.
 */
class scene_renderer_panel final : public editor_panel {

public:

  inline static constexpr auto window_name = ICON_MDI_CHART_BOX_OUTLINE " Scene Renderer###scene_renderer_panel";

  auto draw(editor_state& state) -> void override;

  bool is_open{false};

}; // class scene_renderer_panel

} // namespace editor

#endif // EDITOR_PANELS_SCENE_RENDERER_PANEL_HPP_
