// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_STATIC_MESH_MATERIAL_DRAW_LIST_HPP_
#define LIBSBX_MODELS_STATIC_MESH_MATERIAL_DRAW_LIST_HPP_

#include <cstddef>
#include <filesystem>
#include <unordered_set>
#include <ranges>
#include <algorithm>

#include <libsbx/utility/profiler.hpp>

#include <fmt/format.h>

#include <range/v3/view/enumerate.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/enum.hpp>

#include <libsbx/containers/octree.hpp>

#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/layout.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/draw_list.hpp>

#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/graphics/buffers/uniform_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>
#include <libsbx/graphics/buffers/storage_buffer.hpp>

#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>

#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/point_light.hpp>
#include <libsbx/scenes/components/global_transform.hpp>

#include <libsbx/models/vertex3d.hpp>
#include <libsbx/models/mesh.hpp>
#include <libsbx/models/material.hpp>
#include <libsbx/models/material_draw_list.hpp>

#include <libsbx/sprites/sprites_module.hpp>

namespace sbx::models {

struct static_mesh_traits {

  using mesh_type = models::mesh;
  struct instance_payload {
    std::uint32_t lod_level{0u};
  };

  template<typename DarwList>
  static auto create_shared_buffers([[maybe_unused]] DarwList& draw_list) -> void {

  }

  template<typename DarwList>
  static auto destroy_shared_buffers([[maybe_unused]] DarwList& draw_list) -> void {

  }

  template<typename DarwList>
  static auto update_shared_buffers([[maybe_unused]] DarwList& draw_list) -> void {

  }

  template<typename Callable>
  static void for_each_submission(scenes::scene& scene, Callable&& callable) {
    auto& assets_module = core::engine::get_module<assets::assets_module>();
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

    auto& graph = scene.graph();
    auto& environment = scene.environment();

    auto& assets = scenes_module.asset_registry();

    auto camera_node = environment.camera();
    const auto camera_position = math::vector3{graph.world_position(camera_node)};

    auto query = graph.query<const scenes::static_mesh>();

    for (auto&& [node, static_mesh] : query.each()) {
      const auto transform_data = models::transform_data{graph.world_transform(node), graph.world_normal(node)};
      const auto world_position = graph.world_position(node);

      const auto distance_sq = math::vector3::distance_squared(camera_position, world_position);

      const auto& mesh_id = static_mesh.mesh_id();

      if (mesh_id == math::uuid::nil()) {
        continue;
      }

      const auto& mesh = assets_module.get_asset<models::mesh>(mesh_id);

      for (const auto& submesh : static_mesh.submeshes()) {
        if (submesh.material == math::uuid::nil()) {
          continue;
        }

        const auto base_index = mesh.find_base_submesh_index(submesh.index).value_or(submesh.index);
        const auto lod = _select_lod(distance_sq, mesh.lod_count(base_index));
        const auto actual_index = base_index + lod;

        std::invoke(callable, node, mesh_id, actual_index, submesh.material, transform_data, instance_payload{lod});
      }
    }
  }

  static auto make_instance_data(const scenes::node node, const std::uint32_t transform_index, std::uint32_t material_index, [[maybe_unused]] const instance_payload& payload) -> instance_data {
    return instance_data{transform_index, material_index, static_cast<std::uint32_t>(node), payload.lod_level};
  }

  template<typename Mesh, typename Emitter>
  static auto build_draw_commands(const Mesh& mesh, std::uint32_t submesh_index, std::vector<models::instance_data>&& instances, Emitter&& emitter) -> std::uint32_t {
    if (instances.empty()) {
      return 0;
    }

    const auto& submesh = mesh.submesh(submesh_index);

    auto command = VkDrawIndexedIndirectCommand{};
    command.indexCount = submesh.index_count;
    command.instanceCount = static_cast<std::uint32_t>(instances.size());
    command.firstIndex = submesh.index_offset;
    command.vertexOffset = static_cast<std::int32_t>(submesh.vertex_offset);
    command.firstInstance = emitter.base_instance;

    emitter.emit_instanced(command, std::move(instances));

    return command.instanceCount;
  }

private:

  static auto _select_lod(std::float_t distance_sq, std::uint32_t lod_count) -> std::uint32_t {
    static constexpr auto thresholds = std::array<std::float_t, 4u>{
      900.0f,    // LOD 0: < 30m
      6400.0f,   // LOD 1: < 80m
      22500.0f,  // LOD 2: < 150m
      90000.0f   // LOD 3: < 300m
    };

    auto level = std::uint32_t{0u};

    for (auto i = std::uint32_t{0u}; i < static_cast<std::uint32_t>(thresholds.size()); ++i) {
      if (distance_sq > thresholds[i]) {
        level = i + 1u;
      }
    }

    return std::min(level, lod_count - 1u);
  }

}; // static_mesh_traits

using static_mesh_material_draw_list = basic_material_draw_list<static_mesh_traits>;

} // namespace sbx::models

#endif // LIBSBX_MODELS_STATIC_MESH_MATERIAL_DRAW_LIST_HPP_
