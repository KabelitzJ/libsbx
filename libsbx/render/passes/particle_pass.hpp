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
 * @brief Draws particle billboards, meshes, trails, and GPU-path particles, bucketed into two
 * attachment groups by blend mode.
 *
 * Group 0 (alpha_blend) writes into the weighted-OIT accumulator/revealage pair so
 * transparent_resolve_pass composites it with ordinary transparent meshes. Group 1 (additive)
 * blends (one, one) directly onto resources.color_msaa, since weighted-OIT averages overlapping
 * colors instead of summing them -- backwards for additive glow.
 *
 * Runs between transparent_accumulate_pass and transparent_resolve_pass: alpha_blend output must
 * land in the accumulator before it resolves, and additive output must already be in
 * resources.color before transparent_resolve_pass composites over it.
 *
 * Mesh particles (instanced, unlit) and trail particles (non-instanced, vertex-pulled, width baked
 * into vertex position) share the same two groups, as do GPU-path particles
 * (assets::particle_simulation_mode::gpu), drawn via draw_indirect sized by
 * particle_simulate_pass's prepare_indirect_draw stage.
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

  // [0] = alpha_blend (OIT accumulator/revealage, group 0), [1] = additive (direct color, group 1).
  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _billboard_pipelines{};
  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _mesh_pipelines{};
  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _trail_pipelines{};

  // GPU-path particles (shaders/particles/draw.slang), same two groups as the arrays above --
  // draws context.particle_{additive,alpha}_draw_args via draw_indirect instead of a CPU-known
  // instance count. See libsbx/render/particles/particle_simulate_pass.hpp for what fills these in.
  std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u> _gpu_particle_pipelines{};

  // One buffer per frame-in-flight slot, grown geometrically as the live particle count grows --
  // same reasoning as debug_draw_pass's buffers (the instance count varies frame to frame). Shared
  // by both groups; uploaded at most once per frame regardless of which group's execute() runs first.
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
