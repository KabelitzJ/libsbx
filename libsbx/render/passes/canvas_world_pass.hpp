// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PASSES_CANVAS_WORLD_PASS_HPP_
#define LIBSBX_RENDER_PASSES_CANVAS_WORLD_PASS_HPP_

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

#include <libsbx/canvas/canvas_draw_list.hpp>

namespace sbx::render {

class canvas_world_pass final : public graphics_pass {

public:

  explicit canvas_world_pass(graphics::samples samples);

  ~canvas_world_pass() override;

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Canvas World";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context, std::uint32_t group) const -> bool override;

private:

  auto _draw(render_context& context, graphics::graphics_pipeline& pipeline, std::array<graphics::buffer_handle, graphics::swapchain::max_frames_in_flight>& buffers, std::array<std::size_t, graphics::swapchain::max_frames_in_flight>& capacities, const std::vector<canvas::canvas_vertex>& vertices) -> void;

  memory::observer_ptr<graphics::graphics_pipeline> _pipeline{nullptr};
  memory::observer_ptr<graphics::graphics_pipeline> _text_pipeline{nullptr};

  std::array<graphics::buffer_handle, graphics::swapchain::max_frames_in_flight> _buffers{};
  std::array<std::size_t, graphics::swapchain::max_frames_in_flight> _capacities{};

  std::array<graphics::buffer_handle, graphics::swapchain::max_frames_in_flight> _glyph_buffers{};
  std::array<std::size_t, graphics::swapchain::max_frames_in_flight> _glyph_capacities{};

  graphics::samples _samples;

}; // class canvas_world_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PASSES_CANVAS_WORLD_PASS_HPP_
