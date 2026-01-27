// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ANIMATIONS_SKINNED_MESH_SUBRENDERER_HPP_
#define LIBSBX_ANIMATIONS_SKINNED_MESH_SUBRENDERER_HPP_

#include <filesystem>
#include <unordered_set>
#include <ranges>
#include <algorithm>
#include <iterator>
#include <ranges>
#include <algorithm>

#include <easy/profiler.h>

#include <fmt/format.h>

#include <range/v3/view/enumerate.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/memory/monotonic_arena.hpp>

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
#include <libsbx/core/profiler.hpp>

#include <libsbx/devices/input.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>
#include <libsbx/graphics/buffers/uniform_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>
#include <libsbx/graphics/images/image2d.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>

#include <libsbx/scenes/components/skinned_mesh.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/tag.hpp>
#include <libsbx/scenes/components/point_light.hpp>
#include <libsbx/scenes/components/global_transform.hpp>

#include <libsbx/models/material_draw_list.hpp>
#include <libsbx/models/vertex3d.hpp>

#include <libsbx/animations/vertex3d.hpp>
#include <libsbx/animations/mesh.hpp>
#include <libsbx/animations/animator.hpp>

namespace sbx::animations {

struct skinned_mesh_traits {

  using mesh_type = animations::mesh;
  struct instance_payload { };

  struct skinning_job {
    graphics::buffer::address_type pre_vertices;
    graphics::buffer::address_type post_vertices;
    std::uint32_t vertex_count;
    std::uint32_t bone_offset;
  }; // struct skinning_job

  inline static const auto bone_matrices_buffer_name = utility::hashed_string{"bone_matrices"};

  template<typename DrawList>
  static auto create_shared_buffers(DrawList& draw_list) -> void {
    draw_list.create_buffer(bone_matrices_buffer_name, graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  }

  template<typename DrawList>
  static auto destroy_shared_buffers([[maybe_unused]] DrawList& draw_list) -> void {
    draw_list.destroy_buffer(bone_matrices_buffer_name);
  }

  template<typename DrawList>
  static auto update_shared_buffers(DrawList& draw_list) -> void {
    draw_list.update_buffer(_bone_matrices, bone_matrices_buffer_name);

    _bone_matrices.clear();
  }

  template<typename Callable>
  static auto for_each_submission(scenes::scene& scene, Callable&& callable) -> void {
    auto& assets_module = core::engine::get_module<assets::assets_module>();

    _skinning_jobs.clear();

    // pull id to optionally pack selection; animator is present but we only need the pose already stored in component
    const auto query = scene.query<const scenes::skinned_mesh, animations::animator>();

    for (auto&& [node, skinned_mesh, animator] : query.each()) {
      const auto transform_data = models::transform_data{scene.world_transform(node), scene.world_normal(node)};

      const auto& pose = skinned_mesh.pose();

      const auto bone_offset = static_cast<std::uint32_t>(_bone_matrices.size());

      utility::append(_bone_matrices, pose);

      const auto& mesh_id = skinned_mesh.mesh_id();

      auto& mesh = assets_module.get_asset<animations::mesh>(mesh_id);

      _skinning_jobs.emplace_back(skinning_job{
        .pre_vertices = mesh.address(),
        .post_vertices = 0,
        .vertex_count = mesh.vertex_count(),
        .bone_offset = bone_offset
      });

      for (const auto& submesh : skinned_mesh.submeshes()) {
        std::invoke(callable, node, mesh_id, submesh.index, submesh.material, transform_data, instance_payload{});
      }
    }
  }

  static auto make_instance_data(const scenes::node node, const std::uint32_t transform_index, std::uint32_t material_index, [[maybe_unused]] const instance_payload& payload) -> models::instance_data {
    return models::instance_data{transform_index, material_index, static_cast<std::uint32_t>(node), 0u};
  }

  template<typename Mesh, typename Emitter>
  static auto build_draw_commands(const Mesh& mesh, std::uint32_t submesh_index, std::vector<models::instance_data>&& instances, Emitter&& emitter) -> std::uint32_t {
    auto base_instance_offset = std::uint32_t{0};

    const auto& submesh = mesh.submesh(submesh_index);

    for (const auto& instance : instances) {
      const auto instance_index = emitter.base_instance + base_instance_offset;

      auto command = VkDrawIndexedIndirectCommand{};
      command.indexCount = submesh.index_count;
      command.instanceCount = 1u;
      command.firstIndex = submesh.index_offset;
      command.vertexOffset = static_cast<std::int32_t>(instance_index * mesh.vertex_count());
      command.firstInstance = instance_index;

      emitter.emit_single(command, instance);

      ++base_instance_offset;
    }

    return base_instance_offset;
  }

  static auto skinning_jobs() -> std::vector<skinning_job>& {
    return _skinning_jobs;
  }

private:

  inline static auto _bone_matrices = std::vector<math::matrix4x4>{};
  inline static auto _skinning_jobs = std::vector<skinning_job>{};

}; // struct skinned_mesh_traits

using skinned_mesh_material_draw_list = models::basic_material_draw_list<skinned_mesh_traits>;

class skinned_mesh_subrenderer final : public graphics::subrenderer {

  inline static const auto pipeline_definition = graphics::pipeline_definition{
    .depth = graphics::depth::read_write,
    .uses_transparency = false,
    .rasterization_state = graphics::rasterization_state{
      .polygon_mode = graphics::polygon_mode::fill,
      .cull_mode = graphics::cull_mode::back,
      .front_face = graphics::front_face::counter_clockwise
    }
  };

public:

  skinned_mesh_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline, const skinned_mesh_material_draw_list::bucket bucket);

  ~skinned_mesh_subrenderer() override;

  auto render(graphics::command_buffer& command_buffer) -> void override;

private:

  struct pipeline_data {

    graphics::graphics_pipeline_handle pipeline;
    graphics::push_handler push_handler;
    graphics::descriptor_handler scene_descriptor_handler;

    graphics::compute_pipeline_handle skinning_pipeline;

    pipeline_data(const graphics::graphics_pipeline_handle& handle);

  }; // struct pipeline_data

  auto _get_or_create_pipeline(const models::material_key& key) -> pipeline_data&;

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

  // per-alpha fragment entry points (same scheme as your static renderer)
  inline static const auto _fs_entry = std::array<std::string, 3u>{
    "opaque_main",  // alpha_mode::opaque
    "mask_main",    // alpha_mode::mask
    "blend_main"    // alpha_mode::blend
  };

  std::vector<graphics::attachment_description> _attachments;
  std::filesystem::path _base_pipeline;
  skinned_mesh_material_draw_list::bucket _bucket;
  
  graphics::storage_buffer_handle _skinning_vertex_buffer;
  graphics::storage_buffer_handle _skinning_jobs_buffer;

  sbx::graphics::compute_pipeline _skinning_pipeline;
  sbx::graphics::push_handler _skinning_pipeline_push_handler;

  inline static std::unordered_map<models::material_key, pipeline_data, models::material_key_hash> _pipeline_cache{};
  
}; // class skinned_mesh_subrenderer

} // namespace sbx::animation

#endif // LIBSBX_ANIMATIONS_SKINNED_MESH_SUBRENDERER_HPP_
