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
 * @brief Draws context.packet->particle_billboard_commands as camera-facing, vertex-pulled quads
 * (see shaders/particles/particle_billboard.slang). Declares two attachment groups, mirroring how
 * shadow_pass declares one per cascade:
 *
 * - Group 0 (alpha_blend particles): the same weighted-OIT accumulator/revealage pair
 *   transparent_accumulate_pass writes, so transparent_resolve_pass composites them together with
 *   ordinary transparent meshes with no resolve code of its own.
 * - Group 1 (additive particles): writes directly onto the real scene color target
 *   (resources.color_msaa, resolved into resources.color), blended (one, one, add) -- true additive
 *   glow, not routed through the OIT accumulator. Weighted-OIT's accumulator/revealage math computes
 *   an *averaged* color before compositing it with the standard OVER operator; overlapping additive
 *   particles fed through it converge on their own color instead of adding up brighter, which is
 *   exactly backwards for a glow effect. Unity's particle renderer has the same reasoning: additive
 *   particles blend straight onto the camera target with a literal `Blend One One`.
 *
 * Runs between transparent_accumulate_pass and transparent_resolve_pass, so alpha_blend particles
 * land in the OIT accumulator before it resolves, and additive particles are already sitting in
 * resources.color by the time transparent_resolve_pass composites ordinary transparent geometry over
 * it -- additive glow reads as "behind" translucent surfaces like glass, which is the common choice.
 *
 * Also draws context.packet->particle_mesh_commands as ordinary instanced meshes (unlit -- texture *
 * color, no lighting, matching the billboard particles' own unlit style) into the same two groups,
 * bucketed by blend mode the same way the billboards are; and context.packet->trail_commands as
 * plain (non-instanced) vertex-pulled triangle lists (shaders/particles/trail.slang) -- the ribbon
 * width extrusion is already baked into each vertex's position at extraction time, so this shader
 * needs no per-instance data at all, unlike the billboard/mesh paths.
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

}; // class particle_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PARTICLE_PASS_HPP_
