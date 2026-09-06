// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PASSES_CANVAS_PASS_HPP_
#define LIBSBX_RENDER_PASSES_CANVAS_PASS_HPP_

#include <array>
#include <cstddef>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Draws whatever canvas::canvas_module has resolved into its canvas_draw_list this frame,
 * as one triangle_list draw straight into final_image, then clears it.
 *
 * Runs after tonemap_pass, which is final_image's first touch this frame -- so this group's
 * attachment loads tonemap's result instead of clearing it (see the render graph's own
 * first-touch-clears-otherwise-loads convention, e.g. tonemap_pass::declare's doc comment). No
 * depth attachment: v1 only supports screen-space-overlay UI, which never needs to depth-test
 * against the scene (see canvas::canvas_module's own doc comment for what world_space would add).
 */
class canvas_pass final : public graphics_pass {

public:

  canvas_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Canvas";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context, std::uint32_t group) const -> bool override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

  // One buffer per frame-in-flight slot, grown geometrically (never shrunk) as the UI's vertex
  // count grows -- same pattern as debug_draw_pass, since a canvas_draw_list's vertex count varies
  // frame to frame just like debug_draw's does.
  std::array<graphics::buffer_handle, graphics::swapchain::max_frames_in_flight> _buffers{};
  std::array<std::size_t, graphics::swapchain::max_frames_in_flight> _capacities{};

}; // class canvas_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PASSES_CANVAS_PASS_HPP_
