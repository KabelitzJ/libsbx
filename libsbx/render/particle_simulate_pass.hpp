// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_PARTICLE_SIMULATE_PASS_HPP_
#define LIBSBX_RENDER_PARTICLE_SIMULATE_PASS_HPP_

#include <cstdint>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/render/particle_pool.hpp>

namespace sbx::render {

/**
 * @brief Drives the 4-stage GPU particle compute chain (build_dispatch_args -> simulate -> emit ->
 * prepare_indirect_draw) for both pools, once per render frame.
 *
 * Deliberately NOT a render_pass: render_pass::execute(render_context&) records into the single
 * command buffer render_module's fixed _passes list shares for the frame, submitted once at the
 * end via frame_context::end_frame(). That's fine for every existing pass because their buffers are
 * either host-rewritten every frame or fully GPU-recomputed fresh every frame — but the particle
 * pool's core buffers (particles/dead_list/alive_list/counters) are genuinely read-modify-written
 * *across* frames, and same-queue submission order alone does not guarantee frame N's reads happen
 * after frame N-1's writes complete (GPUs are free to overlap consecutive submissions for
 * throughput — that's the entire point of frames-in-flight). So this owns its own command buffer
 * and its own vkQueueSubmit, gated by its own dedicated timeline semaphore:
 *
 *  - execute() waits on its own timeline == the value the *previous* particle frame signalled,
 *    at COMPUTE_SHADER stage, before touching the pool buffers — this is what actually prevents
 *    the cross-frame race described above.
 *  - execute() runs before frame_context::begin_frame() (see render_module::_consume_packet), so
 *    its GPU work is already queued by the time the main frame's command buffer starts recording.
 *  - The main frame's particle_draw_pass reads the same buffers later that same frame, so
 *    render_module must call frame_context::add_wait(timeline(), <returned signaled_value>, ...)
 *    before frame_context::end_frame() runs — otherwise the draw could race this frame's own
 *    simulate/emit writes.
 *
 * See /home/kaj/.claude/plans/i-want-to-implement-memoized-journal.md's "Cross-frame GPU
 * synchronization" section for the full reasoning.
 */
class particle_simulate_pass : public utility::noncopyable {

public:

  struct result {
    std::uint64_t signaled_value{0u};
    // Which of each pool's two alive_list buffers holds the alive list this particle frame just
    // finished building — particle_draw_pass renders from this one. Flips every frame; shared
    // across both pools since one execute() call advances both in lockstep.
    std::uint32_t write_index{0u};
  }; // struct result

  // One emit.slang dispatch's worth of work: an active emitter instance slot and how many
  // particles it should spawn this frame. particles_to_emit must already be written into the
  // pool's emitter_instances buffer at emitter_index (render_module does both from the same host
  // data) — it's repeated here only so this pass can size the dispatch without reading it back.
  struct emit_request {
    std::uint32_t emitter_index{0u};
    std::uint32_t particles_to_emit{0u};
  }; // struct emit_request

  particle_simulate_pass();

  ~particle_simulate_pass();

  [[nodiscard]] auto timeline() const noexcept -> VkSemaphore {
    return _timeline;
  }

  /**
   * @brief Records and submits one particle frame for both pools. Call once per render frame,
   * before frame_context::begin_frame().
   */
  [[nodiscard]] auto execute(
    particle_pool& additive_pool, std::span<const emit_request> additive_emits,
    particle_pool& alpha_pool, std::span<const emit_request> alpha_emits,
    std::float_t dt, std::float_t time
  ) -> result;

private:

  auto _record_pool(graphics::command_buffer& command_buffer, particle_pool& pool, std::span<const emit_request> emits, std::float_t dt, std::float_t time, std::uint32_t read_index, std::uint32_t write_index) -> void;

  auto _wait_timeline(std::uint64_t value) const -> void;

  // Command buffers are allocated from a command_pool keyed by the allocating thread's id (see
  // graphics_module::command_pool) and must only ever be recorded/reset from that same thread.
  // particle_simulate_pass is constructed on the main thread (inside render_module's constructor,
  // before the render thread starts), but execute() always runs on the render thread — so
  // allocating _command_buffers eagerly in the constructor would hand them a main-thread command
  // pool and then record into them from the render thread, which is what was corrupting the
  // validation layer's internal state. Deferred to first execute() instead, the same lazy-on-first-
  // use pattern frame_context::begin_frame() already uses for its own per-frame command buffers.
  auto _ensure_command_buffers() -> void;

  bool _command_buffers_initialized{false};

  VkSemaphore _timeline{};
  std::uint64_t _frame_index{1u};

  std::vector<graphics::command_buffer> _command_buffers{};

  memory::observer_ptr<graphics::compute_pipeline> _build_dispatch_args_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _simulate_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _emit_pipeline{};
  memory::observer_ptr<graphics::compute_pipeline> _prepare_indirect_draw_pipeline{};

}; // class particle_simulate_pass

} // namespace sbx::render

#endif // LIBSBX_RENDER_PARTICLE_SIMULATE_PASS_HPP_
