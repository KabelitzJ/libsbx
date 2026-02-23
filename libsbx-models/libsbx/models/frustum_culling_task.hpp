// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_FRUSTUM_CULL_TASK_HPP_
#define LIBSBX_MODELS_FRUSTUM_CULL_TASK_HPP_

#include <cstdint>
#include <filesystem>
#include <vector>
#include <array>

#include <vulkan/vulkan.h>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/plane.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/task.hpp>
#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/buffers/storage_buffer.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

namespace sbx::models {

struct aabb {
  math::vector4f min; // xyz = world-space min, w = unused
  math::vector4f max; // xyz = world-space max, w = unused
}; // struct aabb

class frustum_cull_task final : public graphics::task {

public:

  frustum_cull_task(const std::filesystem::path& path);

  ~frustum_cull_task() override;

  auto submit_commands(const std::vector<VkDrawIndexedIndirectCommand>& commands, const std::vector<aabb>& bounds) -> void;

  auto execute(graphics::command_buffer& command_buffer) -> void override;

  auto output_command_buffer_handle() const -> graphics::storage_buffer_handle;

  auto draw_count_buffer_handle() const -> graphics::storage_buffer_handle;

  auto submitted_command_count() const -> std::uint32_t;

private:

  template<typename Type>
  static auto _resize_buffer(graphics::storage_buffer& buffer, std::uint32_t element_count) -> void {
    const auto required_size = static_cast<std::size_t>(element_count) * sizeof(Type);

    if (buffer.size() < required_size) {
      buffer.resize(required_size + required_size / 2);
    }
  }

  template<typename Type>
  static auto _update_buffer(graphics::storage_buffer& buffer, const std::vector<Type>& data) -> void {
    _resize_buffer<Type>(buffer, static_cast<std::uint32_t>(data.size()));

    if (!data.empty()) {
      buffer.update(data.data(), data.size() * sizeof(Type));
    }
  }

  graphics::storage_buffer_handle _input_commands_buffer{};
  graphics::storage_buffer_handle _bounds_buffer{};
  graphics::storage_buffer_handle _output_commands_buffer{};
  graphics::storage_buffer_handle _draw_count_buffer{};

  graphics::compute_pipeline _pipeline;
  graphics::push_handler _push_handler;

  std::uint32_t _command_count{0};

}; // class frustum_cull_task

} // namespace sbx::models

#endif // LIBSBX_MODELS_FRUSTUM_CULL_TASK_HPP_