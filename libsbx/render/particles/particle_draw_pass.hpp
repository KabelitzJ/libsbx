// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PARTICLE_DRAW_PASS_HPP_
#define LIBSBX_RENDER_PARTICLE_DRAW_PASS_HPP_

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>

namespace sbx::render {

/**
 * @brief Draws both particle pools as camera-facing billboards (procedural quad, no vertex/index
 * buffer — shaders/particles/draw.slang's vertex_main), one draw_indirect each, sized by
 * particle_simulate_pass's prepare_indirect_draw stage to that pool's alive count.
 *
 * Runs between transparent_accumulate_pass and transparent_resolve_pass in render_module's
 * _passes list:
 *  - pool[0] (additive) draws straight into the shared color target (continuation write, ONE, ONE,
 *    ADD blend) — additive content is commutative under blending so it never needs OIT, and this
 *    ordering puts it behind whatever transparent_resolve_pass composites on top next, same as
 *    skybox_pass/grid_pass already do for opaque-ish content.
 *  - pool[1] (alpha blend) draws into the *same* accumulator/revealage WBOIT targets
 *    transparent_accumulate_pass just wrote (continuation write, not a fresh clear), reusing its
 *    exact blend state — so mesh transparency and alpha-blend particles get resolved together by
 *    the existing, unmodified transparent_resolve_pass in one pass, with no separate resolve step
 *    and no unsorted-order gap between the two categories.
 *
 * A normal render_pass, like particle_simulate_pass: it only reads the buffers
 * particle_simulate_pass finished writing earlier this same command buffer, handed off via an
 * ordinary intra-frame VkMemoryBarrier2 at the end of that pass's execute() — no semaphore
 * involved for that part. particle_simulate_pass's own cross-*frame* wait (registered on
 * frame_context's timeline) is what actually keeps this pass's reads from racing the *previous*
 * frame's writes to the same buffers.
 */
class particle_draw_pass final : public render_pass {

public:

  particle_draw_pass();

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Particle Draw";
  }

  auto execute(render_context& context) -> void override;

private:

  memory::observer_ptr<graphics::graphics_pipeline> _additive_pipeline{};
  memory::observer_ptr<graphics::graphics_pipeline> _alpha_blend_pipeline{};

}; // class particle_draw_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PARTICLE_DRAW_PASS_HPP_
