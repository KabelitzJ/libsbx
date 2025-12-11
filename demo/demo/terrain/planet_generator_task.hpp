#ifndef DEMO_PLANET_GENERATOR_TASK_HPP_
#define DEMO_PLANET_GENERATOR_TASK_HPP_

#include <libsbx/graphics/task.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>
#include <libsbx/graphics/buffers/uniform_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

namespace demo {

class planet_generator_task final : public sbx::graphics::task {

public:

  planet_generator_task(const std::filesystem::path& path)
  : _pipeline{path},
    _output_vertex_storage_handler{VK_BUFFER_USAGE_VERTEX_BUFFER_BIT},
    _output_index_storage_handler{VK_BUFFER_USAGE_INDEX_BUFFER_BIT},
    _descriptor_handler{_pipeline, 0u} { }

  ~planet_generator_task() override = default;

  auto execute(sbx::graphics::command_buffer& command_buffer) -> void override {
    _pipeline.bind(command_buffer);

    // _uniform_handler.push("model", sbx::math::matrix4x4::identity);
    _uniform_handler.push("subdivisions", 6u);

    _descriptor_handler.push("uniform_parameters", _uniform_handler);
    _descriptor_handler.push("buffer_out_vertices", _output_vertex_storage_handler);
    _descriptor_handler.push("buffer_out_indices", _output_index_storage_handler);

    if (!_descriptor_handler.update(_pipeline)) {
      return;
    }

    _descriptor_handler.bind_descriptors(command_buffer);

    _pipeline.dispatch(command_buffer, {1u, 1u, 1u});
  }

  auto vertices() const noexcept -> const sbx::graphics::storage_buffer& {
    return _output_vertex_storage_handler.storage_buffer();
  }

  auto indices() const noexcept -> const sbx::graphics::storage_buffer& {
    return _output_index_storage_handler.storage_buffer();
  }

private:

  sbx::graphics::compute_pipeline _pipeline;

  sbx::graphics::uniform_handler _uniform_handler;
  sbx::graphics::storage_handler _output_vertex_storage_handler;
  sbx::graphics::storage_handler _output_index_storage_handler;
  sbx::graphics::descriptor_handler _descriptor_handler;

}; // class planet_generator_task

} // namespace demo

#endif // DEMO_PLANET_GENERATOR_TASK_HPP_
