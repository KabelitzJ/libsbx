// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/particle_pool.hpp>

#include <string>
#include <vector>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::render {

particle_pool::particle_pool(const create_info& create_info)
: _max_particles{create_info.max_particles},
  _max_emitter_instances{create_info.max_emitter_instances} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  _particles = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<particle_gpu> * _max_particles,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::device_local,
    .name = create_info.name + " Particles"
  });

  _dead_list = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(std::uint32_t) * _max_particles,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = create_info.name + " Dead List"
  });

  for (auto index = std::uint32_t{0u}; index < 2u; ++index) {
    _alive_list[index] = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = sizeof(std::uint32_t) * _max_particles,
      .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
      .memory = graphics::memory_usage::device_local,
      .name = create_info.name + " Alive List " + std::to_string(index)
    });
  }

  _counters = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(particle_counters_gpu),
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = create_info.name + " Counters"
  });

  _dispatch_args = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(std::uint32_t) * 3u, // VkDispatchIndirectCommand
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage | graphics::buffer_usage::indirect,
    .memory = graphics::memory_usage::device_local,
    .name = create_info.name + " Dispatch Args"
  });

  _draw_args = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(std::uint32_t) * 4u, // VkDrawIndirectCommand
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage | graphics::buffer_usage::indirect,
    .memory = graphics::memory_usage::device_local,
    .name = create_info.name + " Draw Args"
  });

  _emitter_instances = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<emitter_instance_gpu> * _max_emitter_instances,
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = create_info.name + " Emitter Instances"
  });

  _particles_address = registry.get<graphics::buffer>(_particles).address();
  _dead_list_address = registry.get<graphics::buffer>(_dead_list).address();

  for (auto index = std::uint32_t{0u}; index < 2u; ++index) {
    _alive_list_addresses[index] = registry.get<graphics::buffer>(_alive_list[index]).address();
  }

  _counters_address = registry.get<graphics::buffer>(_counters).address();
  _dispatch_args_address = registry.get<graphics::buffer>(_dispatch_args).address();
  _draw_args_address = registry.get<graphics::buffer>(_draw_args).address();
  _emitter_instances_address = registry.get<graphics::buffer>(_emitter_instances).address();

  // One-time init: every slot starts dead, dead_list holds every index so slot 0 is the first one
  // handed out. Both buffers are host_write, so this is a direct mapped write — no queue submission
  // needed, which matters here since the very first particle_simulate_pass submission runs before
  // this frame's main command buffer (and its upload_context::flush) even begins — see
  // particle_pool.hpp's class comment.
  auto initial_dead_list = std::vector<std::uint32_t>(_max_particles);

  for (auto index = std::uint32_t{0u}; index < _max_particles; ++index) {
    initial_dead_list[index] = index;
  }

  registry.get<graphics::buffer>(_dead_list).write(initial_dead_list.data(), sizeof(std::uint32_t) * _max_particles);

  auto initial_counters = particle_counters_gpu{};
  initial_counters.dead_count = _max_particles;
  initial_counters.alive_count = {0u, 0u};

  registry.get<graphics::buffer>(_counters).write(&initial_counters, sizeof(particle_counters_gpu));
}

auto particle_pool::write_emitter_instance(std::uint32_t slot, const emitter_instance_gpu& data) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  registry.get<graphics::buffer>(_emitter_instances).write(&data, sizeof(emitter_instance_gpu), slot * memory::stride_v<emitter_instance_gpu>);
}

} // namespace sbx::render
