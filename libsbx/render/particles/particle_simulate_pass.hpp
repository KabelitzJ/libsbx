// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PARTICLE_SIMULATE_PASS_HPP_
#define LIBSBX_RENDER_PARTICLE_SIMULATE_PASS_HPP_

#include <cstdint>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/render/render_pass.hpp>
#include <libsbx/render/particles/particle_pool.hpp>

namespace sbx::render {

/**
 * @brief Drives the 4-stage GPU particle compute chain (build_dispatch_args -> simulate -> emit ->
 * prepare_indirect_draw) for both pools, once per render frame. A normal render_pass, recording
 * into the same shared per-frame command buffer as everything else — inserted into render_module's
 * _passes right before particle_draw_pass, its only same-frame consumer.
 *
 * The particle pool's core buffers (particles/dead_list/alive_list/counters) are genuinely
 * read-modify-written *across* frames — unlike every other pass's buffers, which are either
 * host-rewritten or fully GPU-recomputed fresh every frame — and same-queue submission order alone
 * does not guarantee frame N's reads happen after frame N-1's writes complete (GPUs are free to
 * overlap consecutive submissions for throughput, which is the entire point of frames-in-flight).
 * That hazard is real, but solving it doesn't need a second command buffer or a second timeline
 * semaphore: frame_context already has one, and its own add_wait() hook is generic. execute()
 * registers a wait on frame_context's own timeline at `frame_index - 1`, scoped to COMPUTE_SHADER
 * stage, before touching the pool buffers — that's what actually prevents the cross-frame race.
 * Scoping the wait to COMPUTE_SHADER means unrelated same-frame work (opaque, lighting, ...) isn't
 * gated behind the previous frame's completion, just the parts that touch the particle pool — see
 * this pass's placement (right before particle_draw_pass, not early in _passes) for why that holds
 * even if a given driver treats a stage-scoped wait more coarsely than the spec's best case allows.
 *
 * The same-frame handoff to particle_draw_pass needs no semaphore at all — it's an ordinary
 * trailing VkMemoryBarrier2 at the end of execute(), the same pattern light_culling_pass already
 * uses to hand its cluster data off to opaque_pass/transparent_accumulate_pass.
 *
 * See /home/kaj/.claude/plans/i-want-to-implement-memoized-journal.md for the full reasoning
 * (including why this used to be a bespoke non-render_pass subsystem, and why that turned out to
 * be unnecessary).
 */
class particle_simulate_pass final : public render_pass {

public:

  particle_simulate_pass(particle_pool& additive_pool, particle_pool& alpha_pool);

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Particle Simulate";
  }

  auto execute(render_context& context) -> void override;

private:

  // One emit.slang dispatch's worth of work: an active emitter instance slot and how many
  // particles it should spawn this frame. particles_to_emit must already be written into the
  // pool's emitter_instances buffer at emitter_index (this pass does both, from the same
  // render_packet::particle_emitters snapshot) — it's repeated here only so the dispatch can be
  // sized without reading it back.
  struct emit_request {
    std::uint32_t emitter_index{0u};
    std::uint32_t particles_to_emit{0u};
  }; // struct emit_request

  auto _record_pool(render_context& context, particle_pool& pool, std::span<const emit_request> emits, std::float_t delta_time, std::float_t time, std::uint32_t read_index, std::uint32_t write_index) -> void;

  particle_pool& _additive_pool;
  particle_pool& _alpha_pool;

  memory::observer_ptr<graphics::compute_pipeline> _build_dispatch_args_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _simulate_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _emit_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _prepare_indirect_draw_pipeline{};

}; // class particle_simulate_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PARTICLE_SIMULATE_PASS_HPP_
