// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PARTICLES_PARTICLE_TASK_HPP_
#define LIBSBX_PARTICLES_PARTICLE_TASK_HPP_

#include <cstdint>
#include <filesystem>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/task.hpp>
#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/buffers/storage_buffer.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/scenes/scene.hpp>

#include <libsbx/particles/particle_emitter.hpp>

namespace sbx::particles {

class particle_task final : public graphics::task {

public:

  particle_task(const std::filesystem::path& path);

  ~particle_task() override = default;

  auto execute(graphics::command_buffer& command_buffer) -> void override;

  auto remove_emitter(scenes::node node) -> void;

  [[nodiscard]] auto particle_buffer(scenes::node node) const -> graphics::storage_buffer_handle;

  [[nodiscard]] auto alive_list_buffer(scenes::node node) const -> graphics::storage_buffer_handle;

  [[nodiscard]] auto indirect_buffer(scenes::node node) const -> graphics::storage_buffer_handle;

private:

  struct alignas(16) emitter_params {
    math::color initial_color;
    math::color end_color;
    math::vector3 emitter_position;
    std::float_t delta_time;
    math::vector3 emission_shape_min;
    std::float_t emission_rate;
    math::vector3 emission_shape_max;
    std::float_t time;
    math::vector3 gravity;
    std::float_t drag;
    math::vector2 initial_speed;
    math::vector2 initial_lifetime;
    math::vector2 initial_size;
    std::float_t end_size_scale;
    std::uint32_t max_particles;
    std::uint32_t emit_count;
    std::uint32_t random_seed;
    std::uint32_t _pad0;
    std::uint32_t _pad1;
  }; // struct emitter_params

  static_assert(alignof(emitter_params) == 16);
  static_assert(sizeof(emitter_params) % 16 == 0);

  struct emitter_gpu_data {
    // Buffers
    graphics::storage_buffer_handle particle_buffer;
    graphics::storage_buffer_handle counter_buffer;
    graphics::storage_buffer_handle alive_list_buffer;
    graphics::storage_buffer_handle indirect_buffer;

    // Descriptors
    graphics::descriptor_handler reset_descriptor;
    graphics::descriptor_handler clear_descriptor;
    graphics::descriptor_handler emit_descriptor;
    graphics::descriptor_handler simulate_descriptor;
    graphics::descriptor_handler prepare_indirect_descriptor;

    // State
    bool reset_requested{true};

    emitter_gpu_data(graphics::compute_pipeline& reset_pipeline,
                     graphics::compute_pipeline& clear_pipeline,
                     graphics::compute_pipeline& emit_pipeline,
                     graphics::compute_pipeline& simulate_pipeline,
                     graphics::compute_pipeline& prepare_indirect_pipeline)
    : reset_descriptor{reset_pipeline, 0u},
      clear_descriptor{clear_pipeline, 0u},
      emit_descriptor{emit_pipeline, 0u},
      simulate_descriptor{simulate_pipeline, 0u},
      prepare_indirect_descriptor{prepare_indirect_pipeline, 0u} { }

  }; // struct emitter_gpu_data

  auto _get_or_create_gpu_data(scenes::node node, const particle_emitter& emitter) -> emitter_gpu_data&;

  auto _initialize_buffers(emitter_gpu_data& gpu_data, const particle_emitter& emitter) -> void;

  auto _build_emitter_params(const particle_emitter& emitter,
                             const math::vector3& position,
                             std::uint32_t emit_count,
                             std::float_t delta_time) -> emitter_params;

  auto _dispatch_reset(graphics::command_buffer& command_buffer,
                       emitter_gpu_data& gpu_data) -> void;

  auto _dispatch_clear(graphics::command_buffer& command_buffer,
                       emitter_gpu_data& gpu_data,
                       const emitter_params& params) -> void;

  auto _dispatch_emit(graphics::command_buffer& command_buffer,
                      emitter_gpu_data& gpu_data,
                      const emitter_params& params) -> void;

  auto _dispatch_simulate(graphics::command_buffer& command_buffer,
                          emitter_gpu_data& gpu_data,
                          const emitter_params& params) -> void;

  auto _dispatch_prepare_indirect(graphics::command_buffer& command_buffer,
                                  emitter_gpu_data& gpu_data) -> void;

  static auto _barrier_compute_to_compute(graphics::command_buffer& command_buffer) -> void;

  static auto _barrier_compute_to_draw(graphics::command_buffer& command_buffer) -> void;

  // Pipelines
  graphics::compute_pipeline _reset_pipeline;
  graphics::compute_pipeline _clear_pipeline;
  graphics::compute_pipeline _emit_pipeline;
  graphics::compute_pipeline _simulate_pipeline;
  graphics::compute_pipeline _prepare_indirect_pipeline;

  // Push handlers
  graphics::push_handler _clear_push_handler;
  graphics::push_handler _emit_push_handler;
  graphics::push_handler _simulate_push_handler;

  // Per-emitter GPU data
  std::unordered_map<scenes::node, emitter_gpu_data> _emitter_gpu_data;

  std::uint32_t _frame_seed{0};

}; // class particle_task

} // namespace sbx::particles

#endif // LIBSBX_PARTICLES_PARTICLE_TASK_HPP_
