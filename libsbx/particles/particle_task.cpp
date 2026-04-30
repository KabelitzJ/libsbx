// SPDX-License-Identifier: MIT
#include <libsbx/particles/particle_task.hpp>

#include <libsbx/math/random.hpp>

#include <libsbx/scenes/scenes_module.hpp>

namespace sbx::particles {

particle_task::particle_task(const std::filesystem::path& path)
: graphics::task{},
  _reset_pipeline{path / "reset_counters"},
  _clear_pipeline{path / "clear_all"},
  _emit_pipeline{path / "emit"},
  _simulate_pipeline{path / "simulate"},
  _prepare_indirect_pipeline{path / "prepare_indirect"},
  _clear_push_handler{_clear_pipeline},
  _emit_push_handler{_emit_pipeline},
  _simulate_push_handler{_simulate_pipeline},
  _frame_seed{math::random::next<std::uint32_t>()} { }

auto particle_task::execute(graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("particle_task::execute");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  auto& scene = scenes_module.active_scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  std::erase_if(_emitter_gpu_data, [&](const auto& entry) {
    const auto& [node, gpu_data] = entry;

    if (!graph.is_valid(node) || !graph.has_component<particle_emitter>(node)) {
      graphics_module.remove_resource<graphics::storage_buffer>(gpu_data.particle_buffer);
      graphics_module.remove_resource<graphics::storage_buffer>(gpu_data.counter_buffer);
      graphics_module.remove_resource<graphics::storage_buffer>(gpu_data.alive_list_buffer);
      graphics_module.remove_resource<graphics::storage_buffer>(gpu_data.indirect_buffer);

      return true;
    }
  
    return false;
  });

  const auto delta_time = core::engine::delta_time().value();

  auto emitter_query = graph.query<particle_emitter, const scenes::transform>();

  for (auto&& [node, emitter, transform] : emitter_query.each()) {
    utility::assert_that(emitter.images.size() > 0u, "Invalid particle_emitter with 0 images");
    utility::assert_that(emitter.images, [](const auto& handle) -> bool { return handle.is_valid(); }, "Invalid image handle in particle_emitter");

    auto& gpu_data = _get_or_create_gpu_data(node, emitter);
    const auto position = graph.world_position(node);

    if (gpu_data.reset_requested) {
      auto params = _build_emitter_params(emitter, position, 0, delta_time);
      _dispatch_clear(command_buffer, gpu_data, params);
      _barrier_compute_to_compute(command_buffer);
      gpu_data.reset_requested = false;
    }

    _dispatch_reset(command_buffer, gpu_data);
    _barrier_compute_to_compute(command_buffer);

    auto emit_count = std::uint32_t{0};

    if (emitter.is_playing()) {
      emitter.elapsed += delta_time;

      if (!emitter.loop && emitter.elapsed >= emitter.duration) {
        emitter.state = emitter_state::stopped;
      } else {
        emitter.emission_accumulator += emitter.emission_rate * delta_time;
        emit_count = static_cast<std::uint32_t>(emitter.emission_accumulator);
        emitter.emission_accumulator -= static_cast<float>(emit_count);
      }
    }

    if (emitter.burst_count > 0) {
      emit_count += emitter.burst_count;
      emitter.burst_count = 0;
    }

    auto params = _build_emitter_params(emitter, position, emit_count, delta_time);

    if (emit_count > 0) {
      _dispatch_emit(command_buffer, gpu_data, params);
      _barrier_compute_to_compute(command_buffer);
    }

    _dispatch_simulate(command_buffer, gpu_data, params);
    _barrier_compute_to_compute(command_buffer);

    _dispatch_prepare_indirect(command_buffer, gpu_data);
    _barrier_compute_to_draw(command_buffer);
  }
}

auto particle_task::remove_emitter(scenes::node node) -> void {
  _emitter_gpu_data.erase(node);
}

auto particle_task::particle_buffer(scenes::node node) const -> graphics::storage_buffer_handle {
  auto it = _emitter_gpu_data.find(node);

  if (it == _emitter_gpu_data.end()) {
    return {};
  }

  return it->second.particle_buffer;
}

auto particle_task::alive_list_buffer(scenes::node node) const -> graphics::storage_buffer_handle {
  auto it = _emitter_gpu_data.find(node);

  if (it == _emitter_gpu_data.end()) {
    return {};
  }

  return it->second.alive_list_buffer;
}

auto particle_task::indirect_buffer(scenes::node node) const -> graphics::storage_buffer_handle {
  auto it = _emitter_gpu_data.find(node);

  if (it == _emitter_gpu_data.end()) {
    return {};
  }

  return it->second.indirect_buffer;
}

auto particle_task::_get_or_create_gpu_data(scenes::node node, const particle_emitter& emitter) -> emitter_gpu_data& {
  auto it = _emitter_gpu_data.find(node);

  if (it != _emitter_gpu_data.end()) {
    return it->second;
  }

  auto [inserted, success] = _emitter_gpu_data.emplace(std::piecewise_construct, std::forward_as_tuple(node), std::forward_as_tuple(_reset_pipeline, _clear_pipeline, _emit_pipeline, _simulate_pipeline, _prepare_indirect_pipeline));

  _initialize_buffers(inserted->second, emitter);

  return inserted->second;
}

auto particle_task::_initialize_buffers(emitter_gpu_data& gpu_data, const particle_emitter& emitter) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  constexpr auto particle_size = 64u;
  const auto particle_buffer_size = emitter.max_particles * particle_size;

  gpu_data.particle_buffer = graphics_module.add_resource<graphics::storage_buffer>(particle_buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  gpu_data.counter_buffer = graphics_module.add_resource<graphics::storage_buffer>(16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  gpu_data.alive_list_buffer = graphics_module.add_resource<graphics::storage_buffer>(emitter.max_particles * sizeof(std::uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  gpu_data.indirect_buffer = graphics_module.add_resource<graphics::storage_buffer>(16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);

  gpu_data.reset_requested = true;
}

auto particle_task::_build_emitter_params(const particle_emitter& emitter, const math::vector3& position, std::uint32_t emit_count, float delta_time) -> emitter_params {
  return emitter_params{
    .initial_color = emitter.initial_color,
    .end_color = emitter.end_color,
    .emitter_position = position,
    .delta_time = delta_time,
    .emission_shape_min = emitter.emission_shape.min(),
    .emission_rate = emitter.emission_rate,
    .emission_shape_max = emitter.emission_shape.max(),
    .time = emitter.elapsed,
    .gravity = emitter.gravity,
    .drag = emitter.drag,
    .initial_speed = emitter.initial_speed,
    .initial_lifetime = emitter.initial_lifetime,
    .initial_size = emitter.initial_size,
    .end_size_scale = emitter.end_size_scale,
    .max_particles = emitter.max_particles,
    .emit_count = emit_count,
    .random_seed = _frame_seed++,
    .texture_count = static_cast<std::uint32_t>(emitter.images.size()),
    ._pad0 = 0,
    ._pad1 = 0,
    ._pad2 = 0
  };
}

auto particle_task::_dispatch_reset(graphics::command_buffer& command_buffer, emitter_gpu_data& gpu_data) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& counters = graphics_module.get_resource<graphics::storage_buffer>(gpu_data.counter_buffer);

  _reset_pipeline.bind(command_buffer);

  gpu_data.reset_descriptor.push("counter", counters);

  gpu_data.reset_descriptor.update(_reset_pipeline);
  gpu_data.reset_descriptor.bind_descriptors(command_buffer);

  _reset_pipeline.dispatch(command_buffer, math::vector3u{1, 1, 1});
}

auto particle_task::_dispatch_clear(graphics::command_buffer& command_buffer, emitter_gpu_data& gpu_data, const emitter_params& params) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& particles = graphics_module.get_resource<graphics::storage_buffer>(gpu_data.particle_buffer);

  _clear_pipeline.bind(command_buffer);

  gpu_data.clear_descriptor.push("particles", particles);

  _clear_push_handler.push(params);

  gpu_data.clear_descriptor.update(_clear_pipeline);
  gpu_data.clear_descriptor.bind_descriptors(command_buffer);
  _clear_push_handler.bind(command_buffer);

  const auto group_count = (params.max_particles + 255) / 256;

  _clear_pipeline.dispatch(command_buffer, math::vector3u{group_count, 1, 1});
}

auto particle_task::_dispatch_emit(graphics::command_buffer& command_buffer, emitter_gpu_data& gpu_data, const emitter_params& params) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& particles = graphics_module.get_resource<graphics::storage_buffer>(gpu_data.particle_buffer);

  _emit_pipeline.bind(command_buffer);

  gpu_data.emit_descriptor.push("particles", particles);

  _emit_push_handler.push(params);

  gpu_data.emit_descriptor.update(_emit_pipeline);
  gpu_data.emit_descriptor.bind_descriptors(command_buffer);
  _emit_push_handler.bind(command_buffer);

  const auto group_count = (params.emit_count + 255) / 256;

  _emit_pipeline.dispatch(command_buffer, math::vector3u{group_count, 1, 1});
}

auto particle_task::_dispatch_simulate(graphics::command_buffer& command_buffer, emitter_gpu_data& gpu_data, const emitter_params& params) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& particles = graphics_module.get_resource<graphics::storage_buffer>(gpu_data.particle_buffer);
  auto& counters = graphics_module.get_resource<graphics::storage_buffer>(gpu_data.counter_buffer);
  auto& alive_list = graphics_module.get_resource<graphics::storage_buffer>(gpu_data.alive_list_buffer);

  _simulate_pipeline.bind(command_buffer);

  gpu_data.simulate_descriptor.push("particles", particles);
  gpu_data.simulate_descriptor.push("counter", counters);
  gpu_data.simulate_descriptor.push("alive_list", alive_list);

  _simulate_push_handler.push(params);

  gpu_data.simulate_descriptor.update(_simulate_pipeline);
  gpu_data.simulate_descriptor.bind_descriptors(command_buffer);
  _simulate_push_handler.bind(command_buffer);

  const auto group_count = (params.max_particles + 255) / 256;

  _simulate_pipeline.dispatch(command_buffer, math::vector3u{group_count, 1, 1});
}

auto particle_task::_dispatch_prepare_indirect(graphics::command_buffer& command_buffer, emitter_gpu_data& gpu_data) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& counters = graphics_module.get_resource<graphics::storage_buffer>(gpu_data.counter_buffer);
  auto& indirect = graphics_module.get_resource<graphics::storage_buffer>(gpu_data.indirect_buffer);

  _prepare_indirect_pipeline.bind(command_buffer);

  gpu_data.prepare_indirect_descriptor.push("counter", counters);
  gpu_data.prepare_indirect_descriptor.push("indirect_args", indirect);

  gpu_data.prepare_indirect_descriptor.update(_prepare_indirect_pipeline);
  gpu_data.prepare_indirect_descriptor.bind_descriptors(command_buffer);

  _prepare_indirect_pipeline.dispatch(command_buffer, math::vector3u{1, 1, 1});
}

auto particle_task::_barrier_compute_to_compute(graphics::command_buffer& command_buffer) -> void {
  auto barrier = VkMemoryBarrier2{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

  auto dependency = VkDependencyInfo{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.memoryBarrierCount = 1;
  dependency.pMemoryBarriers = &barrier;

  vkCmdPipelineBarrier2(command_buffer, &dependency);
}

auto particle_task::_barrier_compute_to_draw(graphics::command_buffer& command_buffer) -> void {
  auto barrier = VkMemoryBarrier2{};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;

  auto dependency = VkDependencyInfo{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.memoryBarrierCount = 1;
  dependency.pMemoryBarriers = &barrier;

  vkCmdPipelineBarrier2(command_buffer, &dependency);
}

} // namespace sbx::particles
