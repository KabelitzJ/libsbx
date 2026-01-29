// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ANIMATIONS_SKINNING_TASK_HPP_
#define LIBSBX_ANIMATIONS_SKINNING_TASK_HPP_

#include <algorithm>
#include <cstdint>
#include <filesystem>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/task.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/buffers/storage_buffer.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/models/vertex3d.hpp>

#include <libsbx/animations/skinned_mesh_subrenderer.hpp>

namespace sbx::animations {

class skinning_task final : public graphics::task {

public:

  skinning_task(const std::filesystem::path& path);

  ~skinning_task() override;

  auto execute(graphics::command_buffer& command_buffer) -> void override;

  auto vertex_buffer_handle() const -> graphics::storage_buffer_handle;

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

  auto _dispatch_skinning(graphics::command_buffer& command_buffer, graphics::buffer::address_type bone_matrices_buffer_address) -> void;

  graphics::storage_buffer_handle _skinning_vertex_buffer{};
  graphics::storage_buffer_handle _skinning_jobs_buffer{};

  graphics::compute_pipeline _skinning_pipeline;
  graphics::push_handler _skinning_pipeline_push_handler;

}; // class skinning_task

} // namespace sbx::animations

#endif // LIBSBX_ANIMATIONS_SKINNING_TASK_HPP_
