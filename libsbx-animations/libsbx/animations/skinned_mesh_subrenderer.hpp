#ifndef LIBSBX_ANIMATIONS_SKINNED_MESH_SUBRENDERER_HPP_
#define LIBSBX_ANIMATIONS_SKINNED_MESH_SUBRENDERER_HPP_

#include <filesystem>
#include <unordered_set>
#include <ranges>
#include <algorithm>
#include <iterator>

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

#include <libsbx/animations/vertex3d.hpp>
#include <libsbx/animations/mesh.hpp>
#include <libsbx/animations/animator.hpp>

namespace sbx::animations {

struct skinned_mesh_traits {

  using component_type = scenes::skinned_mesh;
  using mesh_type = animations::mesh;

  struct instance_payload {
    std::uint32_t bone_offset;
  }; // struct instance_payload

  inline static const auto bone_matrices_buffer_name = utility::hashed_string{"bone_matrices"};

  template<typename DrawList>
  static auto create_shared_buffers(DrawList& draw_list) -> void {
    draw_list.create_buffer(bone_matrices_buffer_name, graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  }

  template<typename DrawList>
  static auto destroy_shared_buffers([[maybe_unused]] DrawList& draw_list) -> void {

  }

  template<typename DrawList>
  static auto update_shared_buffers(DrawList& draw_list) -> void {
    draw_list.update_buffer(_bone_matrices, bone_matrices_buffer_name);
    _bone_matrices.clear();
  }

  template <typename Callable>
  static auto for_each_submission(scenes::scene& scene, Callable&& callable) -> void {
    // pull id to optionally pack selection; animator is present but we only need the pose already stored in component
    const auto query = scene.query<const scenes::skinned_mesh, const scenes::selection_tag, animations::animator>();

    for (auto&& [node, skinned_mesh, selection_tag, animator] : query.each()) {
      const auto transform = models::transform_data{scene.world_transform(node), scene.world_normal(node)};

      const auto bone_offset = static_cast<std::uint32_t>(_bone_matrices.size());
      const auto& pose = skinned_mesh.pose();

      utility::append(_bone_matrices, pose);

      for (const auto& submesh : skinned_mesh.submeshes()) {
        const auto& mesh_id = skinned_mesh.mesh_id();
        const auto submesh_index = submesh.index;
        const auto& material_id = submesh.material;

        std::invoke(callable, skinned_mesh, mesh_id, submesh_index, material_id, transform, selection_tag, instance_payload{bone_offset});
      }
    }
  }

  static auto make_instance_data(std::uint32_t transform_index, std::uint32_t material_index, const scenes::selection_tag& selection_tag, const instance_payload& payload) -> models::instance_data {
    auto [entry, created] = _selection_tags.try_emplace(selection_tag, 0u);

    if (created && selection_tag != scenes::selection_tag::null) {
      entry->second = math::random::next<std::uint32_t>(1u); 
    }

    return models::instance_data{transform_index, material_index, entry->second, payload.bone_offset};
  }

private:

  inline static auto _bone_matrices = std::vector<math::matrix4x4>{};
  inline static auto _selection_tags = std::unordered_map<scenes::selection_tag, std::uint32_t>{};

}; // struct skinned_mesh_traits

using skinned_mesh_material_draw_list = models::basic_material_draw_list<skinned_mesh_traits>;

class skinned_mesh_subrenderer final : public graphics::subrenderer {

  inline static const auto pipeline_definition = graphics::pipeline_definition{
    .depth = graphics::depth::read_write,
    .uses_transparency = false,
    .rasterization_state = graphics::rasterization_state{
      .polygon_mode = graphics::polygon_mode::fill,
      .cull_mode    = graphics::cull_mode::back,
      .front_face   = graphics::front_face::counter_clockwise
    }
  };

public:

  skinned_mesh_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline, const skinned_mesh_material_draw_list::bucket bucket) 
  : graphics::subrenderer{},
    _attachments{attachments},
    _base_pipeline{base_pipeline}, 
    _bucket{bucket} { }

  ~skinned_mesh_subrenderer() override {
    _pipeline_cache.clear();
  }

  auto render(graphics::command_buffer& command_buffer) -> void override {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& renderer = graphics_module.renderer();

    auto& assets_module = core::engine::get_module<assets::assets_module>();
    auto& scene = core::engine::get_module<scenes::scenes_module>().scene();

    // obtain the skinned draw list from the pass (name must match your render graph)
    auto& draw_list = renderer.template draw_list<skinned_mesh_material_draw_list>("skinned_mesh_material");

    for (auto& [key, data] : draw_list.ranges(_bucket)) {
      auto& pipeline_data = _get_or_create_pipeline(key);
      auto& pipeline = graphics_module.get_resource<graphics::graphics_pipeline>(pipeline_data.pipeline);

      pipeline.bind(command_buffer);

      pipeline_data.scene_descriptor_handler.push("scene", scene.uniform_handler());
      pipeline_data.scene_descriptor_handler.push("samplers", draw_list.samplers());
      pipeline_data.scene_descriptor_handler.push("images", draw_list.images());

      if (!pipeline_data.scene_descriptor_handler.update(pipeline)) {
        return;
      }

      pipeline_data.scene_descriptor_handler.bind_descriptors(command_buffer);

      pipeline_data.push_handler.push("transform_data_buffer", draw_list.buffer(skinned_mesh_material_draw_list::transform_data_buffer_name).address());
      pipeline_data.push_handler.push("material_data_buffer", draw_list.buffer(skinned_mesh_material_draw_list::material_data_buffer_name).address());

      pipeline_data.push_handler.push("bone_matrices_buffer", draw_list.buffer(skinned_mesh_traits::bone_matrices_buffer_name).address());

      auto& instance_data_buffer = graphics_module.get_resource<graphics::storage_buffer>(data.instance_data_buffer);
      pipeline_data.push_handler.push("instance_data_buffer", instance_data_buffer.address());

      for (const auto& draw_range : data.ranges) {
        auto& mesh = assets_module.get_asset<animations::mesh>(draw_range.mesh_id);

        mesh.bind(command_buffer);

        pipeline_data.push_handler.push("vertex_buffer", mesh.address());
        pipeline_data.push_handler.bind(command_buffer);

        auto& draw_commands_buffer = graphics_module.get_resource<graphics::storage_buffer>(data.draw_commands_buffer);

        command_buffer.draw_indexed_indirect(draw_commands_buffer, draw_range.range.offset, draw_range.range.count);
      }
    }
  }

private:

  struct pipeline_data {

    graphics::graphics_pipeline_handle pipeline;
    graphics::push_handler push_handler;
    graphics::descriptor_handler scene_descriptor_handler;

    graphics::compute_pipeline_handle skinning_pipeline;

    pipeline_data(const graphics::graphics_pipeline_handle& handle)
    : pipeline{handle},
      push_handler{pipeline}, 
      scene_descriptor_handler{pipeline, 0u} { }

  }; // struct pipeline_data

  auto _get_or_create_pipeline(const models::material_key& key) -> pipeline_data& {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    if (auto it = _pipeline_cache.find(key); it != _pipeline_cache.end()) {
      return it->second;
    }

    auto definition = pipeline_definition;
    definition.depth = graphics::depth::read_write;
    definition.rasterization_state.cull_mode = key.is_double_sided ? graphics::cull_mode::none : graphics::cull_mode::back;
    definition.uses_transparency = (static_cast<models::alpha_mode>(key.alpha) == models::alpha_mode::blend);

    auto& compiler = graphics_module.compiler();

    const auto request = graphics::compiler::compile_request{
      .path = _base_pipeline,
      .per_stage = {
        {SLANG_STAGE_VERTEX, {.entry_point = "skinned_main"}},
        {SLANG_STAGE_FRAGMENT, {.entry_point = _fs_entry.at(key.alpha)}}
      }
    };

    const auto result = compiler.compile(request);

    auto compiled = graphics::graphics_pipeline::compiled_shaders{ _base_pipeline.filename().string(), result.code };
    auto handle = graphics_module.add_resource<graphics::graphics_pipeline>(compiled, _attachments, definition);

    auto [entry, inserted] = _pipeline_cache.emplace(key, handle);

    return entry->second;
  }

  // per-alpha fragment entry points (same scheme as your static renderer)
  inline static const auto _fs_entry = std::array<std::string, 3u>{
    "skinned_opaque_main",  // alpha_mode::opaque
    "skinned_mask_main",    // alpha_mode::mask
    "skinned_blend_main"    // alpha_mode::blend
  };

  std::vector<graphics::attachment_description> _attachments;
  std::filesystem::path _base_pipeline;
  skinned_mesh_material_draw_list::bucket _bucket;

  inline static std::unordered_map<models::material_key, pipeline_data, models::material_key_hash> _pipeline_cache{};
  
}; // class skinned_mesh_subrenderer

} // namespace sbx::animation

#endif // LIBSBX_ANIMATIONS_SKINNED_MESH_SUBRENDERER_HPP_
