// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/particle_simulate_pass.hpp>

#include <array>
#include <cstring>
#include <limits>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::render {

namespace {

auto bind_compute_globals(graphics::command_buffer& command_buffer) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);
}

auto push(graphics::command_buffer& command_buffer, const void* data, std::size_t size) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), data, size);

  command_buffer.push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);
}

auto compute_to_compute_barrier(VkPipelineStageFlags2 extra_dst_stage = 0u, VkAccessFlags2 extra_dst_access = 0u) -> VkMemoryBarrier2 {
  auto barrier = VkMemoryBarrier2{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | extra_dst_stage;
  barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT | extra_dst_access;
  return barrier;
}

} // namespace

particle_simulate_pass::particle_simulate_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& logical_device = graphics_module.logical_device();

  auto type_create_info = VkSemaphoreTypeCreateInfo{};
  type_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  type_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  type_create_info.initialValue = 0u;

  auto semaphore_create_info = VkSemaphoreCreateInfo{};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_create_info.pNext = &type_create_info;

  graphics::validate(vkCreateSemaphore(logical_device, &semaphore_create_info, nullptr, &_timeline), "vkCreateSemaphore");

  logical_device.set_debug_name(_timeline, "Particle Timeline");

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

particle_simulate_pass::~particle_simulate_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& logical_device = graphics_module.logical_device();

  logical_device.wait_idle();

  _command_buffers.clear();

  vkDestroySemaphore(logical_device, _timeline, nullptr);
  _timeline = VK_NULL_HANDLE;
}

auto particle_simulate_pass::_ensure_command_buffers() -> void {
  if (_command_buffers_initialized) {
    return;
  }

  _command_buffers.reserve(graphics::swapchain::max_frames_in_flight);

  for (auto slot = std::uint32_t{0u}; slot < graphics::swapchain::max_frames_in_flight; ++slot) {
    _command_buffers.emplace_back(graphics::queue::type::graphics, false);
  }

  _command_buffers_initialized = true;
}

auto particle_simulate_pass::_wait_timeline(std::uint64_t value) const -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& logical_device = graphics_module.logical_device();

  auto wait_info = VkSemaphoreWaitInfo{};
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wait_info.semaphoreCount = 1u;
  wait_info.pSemaphores = &_timeline;
  wait_info.pValues = &value;

  graphics::validate(vkWaitSemaphores(logical_device, &wait_info, std::numeric_limits<std::uint64_t>::max()), "vkWaitSemaphores");
}

auto particle_simulate_pass::execute(
  particle_pool& additive_pool, std::span<const emit_request> additive_emits,
  particle_pool& alpha_pool, std::span<const emit_request> alpha_emits,
  std::float_t dt, std::float_t time
) -> result {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& logical_device = graphics_module.logical_device();

  // Deferred to here (always the render thread) rather than the constructor (the main thread) —
  // see the class comment on _ensure_command_buffers in the header for why.
  _ensure_command_buffers();

  const auto slot = static_cast<std::uint32_t>(_frame_index % graphics::swapchain::max_frames_in_flight);
  const auto write_index = static_cast<std::uint32_t>(_frame_index % 2u);
  const auto read_index = 1u - write_index;

  // Throttle CPU re-recording of a command buffer slot no more than max_frames_in_flight particle
  // frames ahead of the GPU — same reasoning as frame_context::begin_frame's own throttle, just
  // against this pass's own timeline instead of the frame timeline.
  if (_frame_index > graphics::swapchain::max_frames_in_flight) {
    _wait_timeline(_frame_index - graphics::swapchain::max_frames_in_flight);
  }

  auto& command_buffer = _command_buffers[slot];

  command_buffer.reset();
  command_buffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  bind_compute_globals(command_buffer);

  _record_pool(command_buffer, additive_pool, additive_emits, dt, time, read_index, write_index);
  _record_pool(command_buffer, alpha_pool, alpha_emits, dt, time, read_index, write_index);

  command_buffer.end();

  // Waiting on (frame_index - 1) before frame_index has ever signalled anything (the very first
  // call) is trivially satisfied — the semaphore's initial value is 0 and frame_index starts at 1,
  // so the wait target is 0. No special-casing needed.
  const auto wait_value = _frame_index - 1u;
  const auto signal_value = _frame_index;

  const auto wait_semaphores = std::array<VkSemaphore, 1u>{_timeline};
  const auto wait_values = std::array<std::uint64_t, 1u>{wait_value};
  const auto wait_stages = std::array<VkPipelineStageFlags, 1u>{VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};

  const auto signal_semaphores = std::array<VkSemaphore, 1u>{_timeline};
  const auto signal_values = std::array<std::uint64_t, 1u>{signal_value};

  auto timeline_submit_info = VkTimelineSemaphoreSubmitInfo{};
  timeline_submit_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timeline_submit_info.waitSemaphoreValueCount = static_cast<std::uint32_t>(wait_values.size());
  timeline_submit_info.pWaitSemaphoreValues = wait_values.data();
  timeline_submit_info.signalSemaphoreValueCount = static_cast<std::uint32_t>(signal_values.size());
  timeline_submit_info.pSignalSemaphoreValues = signal_values.data();

  const auto command_buffers = std::array<VkCommandBuffer, 1u>{command_buffer.handle()};

  auto submit_info = VkSubmitInfo{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = &timeline_submit_info;
  submit_info.waitSemaphoreCount = static_cast<std::uint32_t>(wait_semaphores.size());
  submit_info.pWaitSemaphores = wait_semaphores.data();
  submit_info.pWaitDstStageMask = wait_stages.data();
  submit_info.commandBufferCount = static_cast<std::uint32_t>(command_buffers.size());
  submit_info.pCommandBuffers = command_buffers.data();
  submit_info.signalSemaphoreCount = static_cast<std::uint32_t>(signal_semaphores.size());
  submit_info.pSignalSemaphores = signal_semaphores.data();

  const auto& graphics_queue = logical_device.queue<graphics::queue::type::graphics>();

  graphics::validate(vkQueueSubmit(graphics_queue, 1u, &submit_info, VK_NULL_HANDLE), "vkQueueSubmit");

  ++_frame_index;

  return result{signal_value, write_index};
}

auto particle_simulate_pass::_record_pool(graphics::command_buffer& command_buffer, particle_pool& pool, std::span<const emit_request> emits, std::float_t dt, std::float_t time, std::uint32_t read_index, std::uint32_t write_index) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  // Stage 1: build_dispatch_args — sizes stage 2's indirect dispatch to *last frame's* alive
  // count (not max_particles) and clears this frame's write-side alive counter.
  {
    struct push_data {
      graphics::buffer::address_type counters;
      graphics::buffer::address_type dispatch_args;
      std::uint32_t read_index;
      std::uint32_t write_index;
    }; // struct push_data

    const auto data = push_data{pool.counters_address(), pool.dispatch_args_address(), read_index, write_index};

    command_buffer.bind_pipeline(*_build_dispatch_args_pipeline);
    push(command_buffer, &data, sizeof(data));
    command_buffer.dispatch(1u, 1u, 1u);
  }

  auto barrier_to_simulate = compute_to_compute_barrier(VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
  command_buffer.memory_dependency(barrier_to_simulate);

  // Stage 2: simulate — indirect dispatch over last frame's alive list. Ages particles out (push
  // to dead_list), integrates survivors, appends them to this frame's alive list.
  {
    struct push_data {
      graphics::buffer::address_type particles;
      graphics::buffer::address_type dead_list;
      graphics::buffer::address_type alive_list_read;
      graphics::buffer::address_type alive_list_write;
      graphics::buffer::address_type counters;
      graphics::buffer::address_type emitters;
      std::float_t dt;
      std::uint32_t read_index;
      std::uint32_t write_index;
      std::uint32_t max_particles;
    }; // struct push_data

    const auto data = push_data{
      pool.particles_address(),
      pool.dead_list_address(),
      pool.alive_list_address(read_index),
      pool.alive_list_address(write_index),
      pool.counters_address(),
      pool.emitter_instances_address(),
      dt,
      read_index,
      write_index,
      pool.max_particles()
    };

    command_buffer.bind_pipeline(*_simulate_pipeline);
    push(command_buffer, &data, sizeof(data));

    auto& dispatch_args_buffer = registry.get<graphics::buffer>(pool.dispatch_args());
    command_buffer.dispatch_indirect(dispatch_args_buffer);
  }

  auto barrier_to_emit = compute_to_compute_barrier();
  command_buffer.memory_dependency(barrier_to_emit);

  // Stage 3: emit — one small dispatch per active emitter instance that has particles to spawn
  // this frame. Every thread claims its own free slot via a single atomic decrement of dead_count,
  // so concurrent spawns from different emitters can never collide on a slot (the old system's bug).
  if (!emits.empty()) {
    static const auto threads_per_group = std::uint32_t{64u};

    command_buffer.bind_pipeline(*_emit_pipeline);

    for (const auto& request : emits) {
      if (request.particles_to_emit == 0u) {
        continue;
      }

      struct push_data {
        graphics::buffer::address_type particles;
        graphics::buffer::address_type dead_list;
        graphics::buffer::address_type alive_list_write;
        graphics::buffer::address_type counters;
        graphics::buffer::address_type emitters;
        std::uint32_t emitter_index;
        std::uint32_t write_index;
        std::uint32_t max_particles;
        std::float_t time;
      }; // struct push_data

      const auto data = push_data{
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

      push(command_buffer, &data, sizeof(data));

      const auto groups = (request.particles_to_emit + threads_per_group - 1u) / threads_per_group;
      command_buffer.dispatch(groups, 1u, 1u);
    }
  }

  auto barrier_to_prepare = compute_to_compute_barrier();
  command_buffer.memory_dependency(barrier_to_prepare);

  // Stage 4: prepare_indirect_draw — sizes this frame's draw_indirect (particle_draw_pass) to the
  // alive list stages 2/3 just finished building.
  {
    struct push_data {
      graphics::buffer::address_type counters;
      graphics::buffer::address_type draw_args;
      std::uint32_t write_index;
    }; // struct push_data

    const auto data = push_data{pool.counters_address(), pool.draw_args_address(), write_index};

    command_buffer.bind_pipeline(*_prepare_indirect_draw_pipeline);
    push(command_buffer, &data, sizeof(data));
    command_buffer.dispatch(1u, 1u, 1u);
  }
}

} // namespace sbx::render
