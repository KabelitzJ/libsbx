// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PARTICLE_PASS_HPP_
#define LIBSBX_RENDER_PARTICLE_PASS_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Draws particle billboards, meshes, trails and GPU-path particles in two groups by blend mode.
 *
 * Group 0 (alpha_blend) writes the weighted-OIT accumulator so transparent_resolve_pass composites it with transparent meshes.
 * Group 1 (additive) blends (one, one) straight onto color_msaa, since weighted OIT averages overlapping colors instead of summing them.
 * Runs between transparent_accumulate_pass and transparent_resolve_pass.
 */
class particle_pass final : public graphics_pass {

public:

  particle_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Particles";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context, std::uint32_t group) const -> bool override;

private:

  // [0] = alpha_blend (group 0), [1] = additive (group 1).
  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _billboard_pipelines{};
  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _mesh_pipelines{};
  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _trail_pipelines{};

  // GPU-path particles, same groups, drawn indirectly from particle_simulate_pass's draw args.
  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _gpu_particle_pipelines{};

  // One buffer per frame slot, grown geometrically; shared by both groups and uploaded at most once per frame.
  std::array<graphics::buffer_handle, graphics::swapchain::max_frames_in_flight> _billboard_buffers{};
  std::array<std::size_t, graphics::swapchain::max_frames_in_flight> _billboard_capacities{};
  std::array<graphics::buffer_handle, graphics::swapchain::max_frames_in_flight> _mesh_buffers{};
  std::array<std::size_t, graphics::swapchain::max_frames_in_flight> _mesh_capacities{};
  std::array<graphics::buffer_handle, graphics::swapchain::max_frames_in_flight> _trail_buffers{};
  std::array<std::size_t, graphics::swapchain::max_frames_in_flight> _trail_capacities{};
  std::uint64_t _uploaded_frame{std::numeric_limits<std::uint64_t>::max()};

  auto _ensure_uploaded(render_context& context) -> void;

  auto _draw_billboards(render_context& context, std::uint32_t group) -> void;

  auto _draw_meshes(render_context& context, std::uint32_t group) -> void;

  auto _draw_trails(render_context& context, std::uint32_t group) -> void;

  auto _draw_gpu_particles(render_context& context, std::uint32_t group) -> void;

}; // class particle_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PARTICLE_PASS_HPP_
