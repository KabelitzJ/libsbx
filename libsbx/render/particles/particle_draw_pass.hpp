// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PARTICLE_DRAW_PASS_HPP_
#define LIBSBX_RENDER_PARTICLE_DRAW_PASS_HPP_

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/render_graph.hpp>

namespace sbx::render {

/**
 * @brief Draws both particle pools as camera-facing billboards (procedural quad, no vertex/index
 * buffer), one draw_indirect each sized by particle_simulate_pass's prepare_indirect_draw stage.
 *
 * Runs between transparent_accumulate_pass and transparent_resolve_pass: pool[0] (additive) draws
 * straight into the shared color target (ONE, ONE, ADD blend — commutative, needs no OIT); pool[1]
 * (alpha blend) draws into the same WBOIT accumulator/revealage targets transparent_accumulate_pass
 * just wrote, so both get resolved together by the unmodified transparent_resolve_pass.
 *
 * Reads the buffers particle_simulate_pass wrote earlier this frame via an intra-frame
 * VkMemoryBarrier2; particle_simulate_pass's own cross-frame wait is what keeps reads from racing
 * the previous frame's writes.
 */
class particle_draw_pass final : public graphics_pass {

public:

  // Group indices returned by declare()'s add_group calls, in order — used by should_execute/
  // execute to know which pool's draw_args gate/feed a given group.
  inline static constexpr auto additive_group = std::uint32_t{0u};
  inline static constexpr auto alpha_blend_group = std::uint32_t{1u};

  particle_draw_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Particle Draw";
  }

  auto declare(graphics_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context, std::uint32_t group) -> void override;

  [[nodiscard]] auto should_execute(const render_context& context, std::uint32_t group) const -> bool override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _additive_pipeline{};
  memory::observer_ptr<graphics::graphics_pipeline> _alpha_blend_pipeline{};

}; // class particle_draw_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PARTICLE_DRAW_PASS_HPP_
