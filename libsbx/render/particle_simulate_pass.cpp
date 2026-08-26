// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/particle_simulate_pass.hpp>

#include <array>
#include <cstring>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

auto compute_to_compute_barrier(VkPipelineStageFlags2 extra_dst_stage = 0u, VkAccessFlags2 extra_dst_access = 0u) -> VkMemoryBarrier2 {
  auto barrier = VkMemoryBarrier2{};

  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | extra_dst_stage;
  barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | extra_dst_access;

  return barrier;
}

particle_simulate_pass::particle_simulate_pass(particle_pool& additive_pool, particle_pool& alpha_pool)
: _additive_pool{additive_pool},
  _alpha_pool{alpha_pool} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto build_dispatch_args_shader = shader_cache.get({"shaders/particles/build_dispatch_args.slang", entry_points});
  _build_dispatch_args_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = build_dispatch_args_shader,
    .name = "Particle Build Dispatch Args"
  });

  const auto simulate_shader = shader_cache.get({"shaders/particles/simulate.slang", entry_points});
  _simulate_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = simulate_shader,
    .name = "Particle Simulate"
  });

  const auto emit_shader = shader_cache.get({"shaders/particles/emit.slang", entry_points});

  _emit_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = emit_shader,
    .name = "Particle Emit"
  });

  const auto prepare_indirect_draw_shader = shader_cache.get({"shaders/particles/prepare_indirect_draw.slang", entry_points});

  _prepare_indirect_draw_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = prepare_indirect_draw_shader,
    .name = "Particle Prepare Indirect Draw"
  });
}

auto particle_simulate_pass::execute(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& frame_context = graphics_module.frame_context();

  frame_context.add_wait(frame_context.timeline(), frame_context.previous_frame_value(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  const auto delta_time = static_cast<std::float_t>(core::engine::delta_time());
  const auto time = static_cast<std::float_t>(core::engine::time());

  const auto write_index = static_cast<std::uint32_t>(context.frame_index % 2u);
  const auto read_index = 1u - write_index;

  auto additive_emits = std::vector<emit_request>{};
  auto alpha_emits = std::vector<emit_request>{};

  for (const auto& snapshot : context.packet->particle_emitters) {
    auto& pool = (snapshot.pool_index == particle_pool_alpha_blend) ? _alpha_pool : _additive_pool;
    pool.write_emitter_instance(snapshot.slot, snapshot.data);

    if (snapshot.data.particles_to_emit == 0u) {
      continue;
    }

    auto& emits = (snapshot.pool_index == particle_pool_alpha_blend) ? alpha_emits : additive_emits;
    emits.push_back(emit_request{snapshot.slot, snapshot.data.particles_to_emit});
  }

  bind_compute_globals(context);

  _record_pool(context, _additive_pool, additive_emits, delta_time, time, read_index, write_index);
  _record_pool(context, _alpha_pool, alpha_emits, delta_time, time, read_index, write_index);

  auto barrier_to_draw = compute_to_compute_barrier(VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);

  context.command_buffer->memory_dependency(barrier_to_draw);
}

struct build_dispatch_args_push_data {
  graphics::buffer::address_type counters;
  graphics::buffer::address_type dispatch_args;
  std::uint32_t read_index;
  std::uint32_t write_index;
}; // struct build_dispatch_args_push_data

struct simulate_push_data {
  graphics::buffer::address_type particles;
  graphics::buffer::address_type dead_list;
  graphics::buffer::address_type alive_list_read;
  graphics::buffer::address_type alive_list_write;
  graphics::buffer::address_type counters;
  graphics::buffer::address_type emitters;
  std::float_t delta_time;
  std::uint32_t read_index;
  std::uint32_t write_index;
  std::uint32_t max_particles;
}; // struct simulate_push_data

struct emit_push_data {
  graphics::buffer::address_type particles;
  graphics::buffer::address_type dead_list;
  graphics::buffer::address_type alive_list_write;
  graphics::buffer::address_type counters;
  graphics::buffer::address_type emitters;
  std::uint32_t emitter_index;
  std::uint32_t write_index;
  std::uint32_t max_particles;
  std::float_t time;
}; // struct emit_push_data

struct prepare_indirect_draw_push_data {
  graphics::buffer::address_type counters;
  graphics::buffer::address_type draw_args;
  std::uint32_t write_index;
}; // struct prepare_indirect_draw_push_data

auto particle_simulate_pass::_record_pool(render_context& context, particle_pool& pool, std::span<const emit_request> emits, std::float_t delta_time, std::float_t time, std::uint32_t read_index, std::uint32_t write_index) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  auto& command_buffer = *context.command_buffer;

  // Stage 1: build_dispatch_args — sizes stage 2's indirect dispatch to *last frame's* alive count (not max_particles) and clears this frame's write-side alive counter.
  command_buffer.bind_pipeline(*_build_dispatch_args_pipeline);

  const auto build_dispatch_args_data = build_dispatch_args_push_data{
    pool.counters_address(), 
    pool.dispatch_args_address(), 
    read_index, 
    write_index
  };

  write_push_constants(context, build_dispatch_args_data);

  command_buffer.dispatch(1u, 1u, 1u);

  auto barrier_to_simulate = compute_to_compute_barrier(VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
  command_buffer.memory_dependency(barrier_to_simulate);

  // Stage 2: simulate — indirect dispatch over last frame's alive list. Ages particles out (push to dead_list), integrates survivors, appends them to this frame's alive list.
  command_buffer.bind_pipeline(*_simulate_pipeline);

  const auto simulate_data = simulate_push_data{
    pool.particles_address(),
    pool.dead_list_address(),
    pool.alive_list_address(read_index),
    pool.alive_list_address(write_index),
    pool.counters_address(),
    pool.emitter_instances_address(),
    delta_time,
    read_index,
    write_index,
    pool.max_particles()
  };

  write_push_constants(context, simulate_data);

  auto& dispatch_args_buffer = registry.get<graphics::buffer>(pool.dispatch_args());

  command_buffer.dispatch_indirect(dispatch_args_buffer);

  auto barrier_to_emit = compute_to_compute_barrier();
  command_buffer.memory_dependency(barrier_to_emit);

  // Stage 3: emit — one small dispatch per active emitter instance that has particles to spawn this frame. Every thread claims its own free slot via a single 
  // atomic decrement of dead_count, so concurrent spawns from different emitters can never collide on a slot (the old system's bug).
  if (!emits.empty()) {
    static const auto threads_per_group = std::uint32_t{64u};

    command_buffer.bind_pipeline(*_emit_pipeline);

    for (const auto& request : emits) {
      if (request.particles_to_emit == 0u) {
        continue;
      }

      const auto emit_data = emit_push_data{
        pool.particles_address(),
        pool.dead_list_address(),
        pool.alive_list_address(write_index),
        pool.counters_address(),
        pool.emitter_instances_address(),
        request.emitter_index,
        write_index,
        pool.max_particles(),
        time
      };

      write_push_constants(context, emit_data);

      const auto groups = (request.particles_to_emit + threads_per_group - 1u) / threads_per_group;

      command_buffer.dispatch(groups, 1u, 1u);
    }
  }

  auto barrier_to_prepare = compute_to_compute_barrier();
  command_buffer.memory_dependency(barrier_to_prepare);

  // Stage 4: prepare_indirect_draw — sizes this frame's draw_indirect (particle_draw_pass) to the alive list stages 2/3 just finished building.
  command_buffer.bind_pipeline(*_prepare_indirect_draw_pipeline);

  const auto prepare_indirect_draw_data = prepare_indirect_draw_push_data{
    pool.counters_address(), 
    pool.draw_args_address(), 
    write_index
  };

  write_push_constants(context, prepare_indirect_draw_data);

  command_buffer.dispatch(1u, 1u, 1u);
}

} // namespace sbx::render
