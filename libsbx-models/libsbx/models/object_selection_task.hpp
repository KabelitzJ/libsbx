#ifndef LIBSBX_MODELS_OBJECT_SELECTION_TASK_HPP_
#define LIBSBX_MODELS_OBJECT_SELECTION_TASK_HPP_

#include <numbers>

#include <libsbx/graphics/task.hpp>
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/components/transform.hpp>
#include <libsbx/math/random.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/graphics/buffers/uniform_handler.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/global_transform.hpp>

namespace sbx::models {

class object_selection_task final : public graphics::task {

  using base = graphics::task;

public:

  object_selection_task(const std::filesystem::path& path)
  : _pipeline{path},
    _push_handler{_pipeline}{
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  }

  ~object_selection_task() override = default;

  auto execute(graphics::command_buffer& command_buffer) -> void override {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

    auto& logical_device = graphics_module.logical_device();

    auto& scene = scenes_module.scene();
  }

private:

  graphics::compute_pipeline _pipeline;

  graphics::storage_buffer_handle _grass_input_buffer;
  graphics::storage_buffer_handle _grass_output_buffer;
  graphics::storage_buffer_handle _draw_command_buffer;  

  graphics::push_handler _push_handler;

}; // class object_selection_task

} // namespace sbx::models

#endif // LIBSBX_MODELS_OBJECT_SELECTION_TASK_HPP_
