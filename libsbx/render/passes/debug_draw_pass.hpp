// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_DEBUG_DRAW_PASS_HPP_
#define LIBSBX_RENDER_DEBUG_DRAW_PASS_HPP_

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
 * @brief Draws whatever scene_renderer_module::debug_draw() has accumulated this frame -- one
 * upload, one line_list draw call for every source (physics colliders today, script gizmos later)
 * -- then clears it. See libsbx/render/debug/debug_draw.hpp for the accumulator itself.
 *
 * Runs right after grid_pass, for the same reason grid_pass runs where it does (see its doc
 * comment): it needs the fully-populated opaque depth buffer to occlude correctly, and it must be
 * part of what transparent_resolve_pass composites against.
 */
class debug_draw_pass final : public graphics_pass {

public:

  debug_draw_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Debug Draw";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context, std::uint32_t group) const -> bool override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};

  // One buffer per frame-in-flight slot, since the vertex count varies frame to frame (unlike the
  // fixed-stride frame_data ring) -- indexed by render_context::slot, grown geometrically (never
  // shrunk) as debug_draw's vertex count grows.
  std::array<graphics::buffer_handle, graphics::swapchain::max_frames_in_flight> _buffers{};
  std::array<std::size_t, graphics::swapchain::max_frames_in_flight> _capacities{};

}; // class debug_draw_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_DEBUG_DRAW_PASS_HPP_
