// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/particle_pool.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::render {

particle_pool::particle_pool(const create_info& create_info)
: _max_particles{create_info.max_particles},
  _max_emitter_instances{create_info.max_emitter_instances},
  _name{create_info.name} {
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
  // needed, which matters here since particle_simulate_pass's compute chain reads them the very
  // first time it records, before any upload_context::flush this frame — see particle_pool.hpp's
  // class comment.
  auto initial_dead_list = std::vector<std::uint32_t>(_max_particles);

  for (auto index = std::uint32_t{0u}; index < _max_particles; ++index) {
    initial_dead_list[index] = index;
  }

  registry.get<graphics::buffer>(_dead_list).write(initial_dead_list.data(), sizeof(std::uint32_t) * _max_particles);

  auto initial_counters = particle_counters_gpu{};
  initial_counters.dead_count = _max_particles;
  initial_counters.alive_count = {0u, 0u};

  registry.get<graphics::buffer>(_counters).write(&initial_counters, sizeof(particle_counters_gpu));

  // Emitter-instance slot allocator: every slot starts free, in ascending order (any order is
  // correct — free_list is a stack — this just makes gpu_slot assignment easier to read while
  // debugging, same reasoning render_module's old free-list init used before this moved here).
  _free_list.resize(_max_emitter_instances);

  for (auto slot = std::uint32_t{0u}; slot < _max_emitter_instances; ++slot) {
    _free_list[slot] = _max_emitter_instances - 1u - slot;
  }

  _drain_timer.assign(_max_emitter_instances, -1.0f);
  _lifetime_max.assign(_max_emitter_instances, 0.0f);
  _claimed_this_frame.assign(_max_emitter_instances, false);
  _claimed_last_frame.assign(_max_emitter_instances, false);
}

auto particle_pool::write_emitter_instance(std::uint32_t slot, const emitter_instance_gpu& data) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  registry.get<graphics::buffer>(_emitter_instances).write(&data, sizeof(emitter_instance_gpu), slot * memory::stride_v<emitter_instance_gpu>);
}

auto particle_pool::claim_slot() -> std::optional<std::uint32_t> {
  if (_free_list.empty()) {
    if (!_exhaustion_logged) {
      utility::logger<"render">::warn("Particle pool '{}' exhausted ({} slots) — new emitters won't spawn until one frees up", _name, _max_emitter_instances);
      _exhaustion_logged = true;
    }

    return std::nullopt;
  }

  const auto slot = _free_list.back();
  _free_list.pop_back();

  return slot;
}

auto particle_pool::keep_alive(std::uint32_t slot, std::float_t lifetime_max) -> void {
  _claimed_this_frame[slot] = true;
  _lifetime_max[slot] = lifetime_max;
}

auto particle_pool::tick(std::float_t dt) -> void {
  for (auto slot = std::uint32_t{0u}; slot < _max_emitter_instances; ++slot) {
    if (_claimed_last_frame[slot] && !_claimed_this_frame[slot]) {
      _drain_timer[slot] = _lifetime_max[slot];
    }

    if (_drain_timer[slot] >= 0.0f) {
      _drain_timer[slot] -= dt;

      if (_drain_timer[slot] <= 0.0f) {
        _drain_timer[slot] = -1.0f;
        _free_list.push_back(slot);
      }
    }
  }

  _claimed_last_frame = _claimed_this_frame;
  std::fill(_claimed_this_frame.begin(), _claimed_this_frame.end(), false);
}

} // namespace sbx::render
