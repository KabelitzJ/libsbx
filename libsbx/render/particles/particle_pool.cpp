// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/particles/particle_pool.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/iterator.hpp>

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
    .size = memory::stride_v<particle> * _max_particles,
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
    .size = sizeof(particle_counters),
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = create_info.name + " Counters"
  });

  _dispatch_args = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(VkDispatchIndirectCommand),
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage | graphics::buffer_usage::indirect,
    .memory = graphics::memory_usage::device_local,
    .name = create_info.name + " Dispatch Args"
  });

  _draw_args = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = sizeof(VkDrawIndirectCommand),
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage | graphics::buffer_usage::indirect,
    .memory = graphics::memory_usage::device_local,
    .name = create_info.name + " Draw Args"
  });

  _emitter_instances = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = memory::stride_v<emitter_instance> * _max_emitter_instances,
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

  _write_initial_state();

  _free_list.resize(_max_emitter_instances);

  for (auto slot = std::uint32_t{0u}; slot < _max_emitter_instances; ++slot) {
    _free_list[slot] = _max_emitter_instances - 1u - slot;
  }

  _drain_timer.assign(_max_emitter_instances, -1.0f);
  _lifetime_max.assign(_max_emitter_instances, 0.0f);
  _claimed_this_frame.assign(_max_emitter_instances, false);
  _claimed_last_frame.assign(_max_emitter_instances, false);
}

auto particle_pool::_write_initial_state() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  auto initial_dead_list = utility::make_vector<std::uint32_t>(_max_particles);

  for (auto index = std::uint32_t{0u}; index < _max_particles; ++index) {
    initial_dead_list[index] = index;
  }

  registry.get<graphics::buffer>(_dead_list).write(initial_dead_list.data(), sizeof(std::uint32_t) * initial_dead_list.size());

  auto initial_counters = particle_counters{};
  initial_counters.dead_count = _max_particles;
  initial_counters.alive_count = {0u, 0u};

  registry.get<graphics::buffer>(_counters).write(&initial_counters, sizeof(particle_counters));
}

auto particle_pool::write_emitter_instance(std::uint32_t slot, const emitter_instance& data) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  registry.get<graphics::buffer>(_emitter_instances).write(&data, sizeof(emitter_instance), slot * memory::stride_v<emitter_instance>);
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

auto particle_pool::tick(std::float_t delta_time) -> void {
  for (auto slot = std::uint32_t{0u}; slot < _max_emitter_instances; ++slot) {
    if (_claimed_last_frame[slot] && !_claimed_this_frame[slot]) {
      _drain_timer[slot] = _lifetime_max[slot];
    }

    if (_drain_timer[slot] >= 0.0f) {
      _drain_timer[slot] -= delta_time;

      if (_drain_timer[slot] <= 0.0f) {
        _drain_timer[slot] = -1.0f;
        _free_list.push_back(slot);
      }
    }
  }

  _claimed_last_frame = _claimed_this_frame;
  std::fill(_claimed_this_frame.begin(), _claimed_this_frame.end(), false);
}

auto particle_pool::clear() -> void {
  _write_initial_state();

  _free_list.resize(_max_emitter_instances);

  for (auto slot = std::uint32_t{0u}; slot < _max_emitter_instances; ++slot) {
    _free_list[slot] = _max_emitter_instances - 1u - slot;
  }

  _drain_timer.assign(_max_emitter_instances, -1.0f);
  _lifetime_max.assign(_max_emitter_instances, 0.0f);
  _claimed_this_frame.assign(_max_emitter_instances, false);
  _claimed_last_frame.assign(_max_emitter_instances, false);
  _exhaustion_logged = false;
}

} // namespace sbx::render
