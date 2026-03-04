// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_FRUSTUM_CULLING_TASK_HPP_
#define LIBSBX_MODELS_FRUSTUM_CULLING_TASK_HPP_

#include <cstdint>
#include <array>
#include <vector>
#include <filesystem>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include <easy/profiler.h>

#include <magic_enum/magic_enum.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/task.hpp>
#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/buffers/storage_buffer.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/components/camera.hpp>

#include <libsbx/models/mesh.hpp>
#include <libsbx/models/material.hpp>
#include <libsbx/models/material_draw_list.hpp>
#include <libsbx/models/static_mesh_material_draw_list.hpp>

namespace sbx::models {

struct local_aabb {
  math::vector4f center;  // xyz = center, w = pad
  math::vector4f extents; // xyz = half-extents, w = pad
}; // struct local_aabb

struct frustum_planes {
  std::array<math::vector4f, 6> planes;
}; // struct frustum_planes

class frustum_culling_task final : public graphics::task {

public:

  static constexpr auto no_cascade = std::uint32_t{0xFFFFFFFF};

  struct culled_range_data {
    graphics::storage_buffer_handle commands_buffer;
    graphics::storage_buffer_handle instances_buffer;
  }; // struct culled_range_data

  frustum_culling_task(const std::filesystem::path& path);

  ~frustum_culling_task() override;

  auto execute(graphics::command_buffer& command_buffer) -> void override;

  auto culled(bucket bucket, const material_key& key, std::uint32_t cascade = no_cascade) const -> const culled_range_data*;

private:

  struct prefix_sum_result {
    std::vector<std::uint32_t> prefix_sum;
    std::uint32_t total_instances;
  }; // struct prefix_sum_result

  struct culled_range_key {
    models::bucket bucket;
    models::material_key key;
    std::uint32_t cascade;

    auto operator==(const culled_range_key& other) const -> bool = default;
  }; // struct culled_range_key

  struct culled_range_key_hash {
    auto operator()(const culled_range_key& key) const -> std::size_t {
      auto seed = std::size_t{0};
      
      utility::hash_combine(seed, key.bucket, key.key, key.cascade);

      return seed;
    }
  }; // struct culled_range_key_hash

  static auto _extract_frustum_planes(const math::matrix4x4f& view_projection) -> frustum_planes;

  auto _get_or_create_culled_range(const culled_range_key& key, VkDeviceSize commands_size, VkDeviceSize instances_size) -> culled_range_data&;

  auto _build_bounds(const static_mesh_material_draw_list::bucket_entry& entry) -> std::vector<local_aabb>;

  auto _build_prefix_sum(const static_mesh_material_draw_list::bucket_entry& entry) -> prefix_sum_result;

  auto _cull_bucket(graphics::command_buffer& command_buffer, bucket current_bucket, std::uint32_t cascade, const frustum_planes& frustum, static_mesh_material_draw_list& draw_list) -> void;

  std::unordered_map<culled_range_key, culled_range_data, culled_range_key_hash> _culled_ranges;

  graphics::storage_buffer_handle _bounds_buffer;
  graphics::storage_buffer_handle _prefix_sum_buffer;
  graphics::storage_buffer_handle _frustum_buffer;

  graphics::compute_pipeline _pipeline;
  graphics::push_handler _push_handler;

}; // class frustum_culling_task

} // namespace sbx::models

#endif // LIBSBX_MODELS_FRUSTUM_CULLING_TASK_HPP_
