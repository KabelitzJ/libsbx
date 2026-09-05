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
#include <libsbx/render/render_graph.hpp>
#include <libsbx/render/particles/particle_pool.hpp>

namespace sbx::render {

/**
 * @brief Drives the 4-stage GPU particle compute chain (build_dispatch_args -> simulate -> emit ->
 * prepare_indirect_draw) for both pools, once per frame — placed right before particle_draw_pass,
 * its only same-frame consumer.
 *
 * Unlike other passes' buffers, the pool buffers are read-modify-written *across* frames, so
 * same-queue submission order alone doesn't prevent frame N's reads racing frame N-1's writes:
 * execute() waits on frame_context's own timeline at `frame_index - 1`, scoped to COMPUTE_SHADER,
 * before touching them. The handoff to particle_draw_pass needs no semaphore — just a trailing
 * VkMemoryBarrier2, the same pattern light_culling_pass uses for its own consumers.
 */
class particle_simulate_pass final : public compute_pass {

public:

  particle_simulate_pass(particle_pool& additive_pool, particle_pool& alpha_pool);

  [[nodiscard]] auto name() const -> std::string_view override {
    return "Particle Simulate";
  }

  auto declare(compute_pass_builder& builder, const graph_resources& resources) -> void override;

  auto execute(render_context& context) -> void override;

private:

  // One emit.slang dispatch's work: emitter_index + spawn count. particles_to_emit is duplicated
  // here (also written into the pool's emitter_instances buffer) only so the dispatch can be sized
  // without reading it back.
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
