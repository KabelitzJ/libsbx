// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SELECTION_OUTLINE_PASS_HPP_
#define LIBSBX_RENDER_SELECTION_OUTLINE_PASS_HPP_

#include <array>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Editor selection outline, a port of Hazel's jump flood outline; a no-op unless the packet has selected_commands.
 *
 * Groups, in order:
 *   mask:      selected_commands into selection_mask, against its own depth, so the outline isn't hidden by other geometry.
 *   init:      seeds jump_flood[0] with which side of the silhouette each pixel is on.
 *   flood:     one step-1 flood into jump_flood[1] (Hazel's step count works out to a single pass).
 *   composite: blends orange where the flooded distance is under ~2 texels onto final_image, after tonemapping.
 */
class selection_outline_pass final : public graphics_pass {

public:

  selection_outline_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Selection Outline";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context, std::uint32_t group) const -> bool override;

private:

  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u> _mask_pipelines{};
  memory::observer_ptr<graphics::graphics_pipeline> _init_pipeline{nullptr};
  memory::observer_ptr<graphics::graphics_pipeline> _flood_pipeline{nullptr};
  memory::observer_ptr<graphics::graphics_pipeline> _composite_pipeline{nullptr};

  // Hazel's (1.0, 0.5, 0.0) is a display value; converted to linear when final_image is sRGB so it shows the same.
  std::array<std::float_t, 3u> _outline_color{1.0f, 0.5f, 0.0f};

}; // class selection_outline_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_SELECTION_OUTLINE_PASS_HPP_
