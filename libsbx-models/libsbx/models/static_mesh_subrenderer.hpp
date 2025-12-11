#ifndef LIBSBX_MODELS_STATIC_MESH_SUBRENDERER_HPP_
#define LIBSBX_MODELS_STATIC_MESH_SUBRENDERER_HPP_

#include <cstddef>
#include <filesystem>
#include <unordered_set>
#include <ranges>
#include <algorithm>

#include <easy/profiler.h>

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
#include <libsbx/scenes/components/selection_tag.hpp>

#include <libsbx/models/vertex3d.hpp>
#include <libsbx/models/mesh.hpp>
#include <libsbx/models/material.hpp>
#include <libsbx/models/material_draw_list.hpp>

namespace sbx::models {

struct static_mesh_traits {

  using component_type = scenes::static_mesh;
  using mesh_type = models::mesh;
  struct instance_payload { };

  template<typename DarwList>
  static auto create_shared_buffers([[maybe_unused]] DarwList& draw_list) -> void {

  }

  template<typename DarwList>
  static auto destroy_shared_buffers([[maybe_unused]] DarwList& draw_list) -> void {

  }

  template<typename DarwList>
  static auto update_shared_buffers([[maybe_unused]] DarwList& draw_list) -> void {

  }

  template<class Callable>
  static void for_each_submission(scenes::scene& scene, Callable&& callable) {
    auto query = scene.query<const component_type, const scenes::selection_tag>();

    for (auto&& [node, component, selection_tag] : query.each()) {
      const auto transform_data = models::transform_data{ scene.world_transform(node), scene.world_normal(node) };

      for (const auto& submesh : component.submeshes()) {
        std::invoke(callable, component, component.mesh_id(), submesh.index, submesh.material, transform_data, selection_tag, instance_payload{});
      }
    }
  }

  static auto make_instance_data(std::uint32_t transform_index, std::uint32_t material_index, const scenes::selection_tag& selection_tag, const instance_payload& payload) -> instance_data {
    auto [entry, created] = _selection_tags.try_emplace(selection_tag, 0u);

    if (created && selection_tag != scenes::selection_tag::null) {
      entry->second = math::random::next<std::uint32_t>(1u); 
    }

    return instance_data{transform_index, material_index, entry->second, 0u};
  }

private:

  inline static auto _selection_tags = std::unordered_map<scenes::selection_tag, std::uint32_t>{};

}; // static_mesh_traits

using static_mesh_material_draw_list = basic_material_draw_list<static_mesh_traits>;

class static_mesh_subrenderer final : public graphics::subrenderer {

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

  static_mesh_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline, const static_mesh_material_draw_list::bucket bucket)
  : graphics::subrenderer{},
    _attachments{attachments},
    _base_pipeline{base_pipeline},
    _bucket{bucket} { }

  ~static_mesh_subrenderer() override {
    _pipeline_cache.clear();
  }

  auto render(graphics::command_buffer& command_buffer) -> void override {
    EASY_FUNCTION();

    SBX_PROFILE_SCOPE("static_mesh_subrenderer::render");

    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& renderer = graphics_module.renderer();

    auto& assets_module = core::engine::get_module<assets::assets_module>();

    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
    auto& scene = scenes_module.scene();

    auto& draw_list = renderer.draw_list<models::static_mesh_material_draw_list>("static_mesh_material");

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

      
      pipeline_data.push_handler.push("transform_data_buffer", draw_list.buffer(static_mesh_material_draw_list::transform_data_buffer_name).address());
      pipeline_data.push_handler.push("material_data_buffer", draw_list.buffer(static_mesh_material_draw_list::material_data_buffer_name).address());

      auto& instance_data_buffer = graphics_module.get_resource<graphics::storage_buffer>(data.instance_data_buffer);

      pipeline_data.push_handler.push("instance_data_buffer", instance_data_buffer.address());

      const auto hash = material_key_hash{}(key);

      for (const auto& [mesh_id, range] : data.ranges) {
        auto& mesh = assets_module.get_asset<models::mesh>(mesh_id);
        
        mesh.bind(command_buffer);
        
        pipeline_data.push_handler.push("vertex_buffer", mesh.address());

        pipeline_data.push_handler.bind(command_buffer);

        auto& draw_commands_buffer = graphics_module.get_resource<graphics::storage_buffer>(data.draw_commands_buffer);

        command_buffer.draw_indexed_indirect(draw_commands_buffer, range.offset, range.count);
      }
    }
  }

private:

  struct pipeline_data {

    graphics::graphics_pipeline_handle pipeline;
    graphics::push_handler push_handler;
    graphics::descriptor_handler scene_descriptor_handler;

    pipeline_data(const graphics::graphics_pipeline_handle& handle)
    : pipeline{handle},
      push_handler{pipeline},
      scene_descriptor_handler{pipeline, 0u} { }

  }; // struct pipeline_data

  auto _get_or_create_pipeline(const material_key& key) -> pipeline_data& {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

    if (auto entry = _pipeline_cache.find(key); entry != _pipeline_cache.end()) {
      return entry->second;
    }

    auto definition = pipeline_definition;
    definition.depth = graphics::depth::read_write;
    definition.rasterization_state.cull_mode = key.is_double_sided ? graphics::cull_mode::none : graphics::cull_mode::back;
    definition.uses_transparency = (static_cast<alpha_mode>(key.alpha) == alpha_mode::blend);

    auto& compiler = graphics_module.compiler();

    const auto request = graphics::compiler::compile_request{
      .path = _base_pipeline,
      .per_stage = {
        {SLANG_STAGE_VERTEX, { .entry_point = "static_main" }},
        {SLANG_STAGE_FRAGMENT, { .entry_point = _entry_point.at(key.alpha) }}
      }
    };

    const auto result = compiler.compile(request);

    auto compiled_shaders = graphics::graphics_pipeline::compiled_shaders{_base_pipeline.filename().string(), result.code};

    auto pipeline = graphics_module.add_resource<graphics::graphics_pipeline>(compiled_shaders, _attachments, definition);

    auto [entry, inserted] = _pipeline_cache.emplace(key, pipeline);

    return entry->second;
  }

  inline static const auto _entry_point = std::array<std::string, 3u>{
    "static_opaque_main",  // alpha_mode::opaque
    "static_mask_main",    // alpha_mode::mask 
    "static_blend_main"    // alpha_mode::blend
  };

  std::vector<graphics::attachment_description> _attachments;
  std::filesystem::path _base_pipeline;
  static_mesh_material_draw_list::bucket _bucket;

  inline static auto _pipeline_cache = std::unordered_map<material_key, pipeline_data, material_key_hash>{};

}; // class static_mesh_subrenderer

} // namespace sbx::models

template<>
struct sbx::utility::enum_mapping<sbx::models::material_feature> {

  using entry_type = sbx::utility::entry<sbx::models::material_feature>;

  static constexpr auto values = std::array<entry_type, 6u>{
    entry_type{sbx::models::material_feature::emission, "emission"},
    entry_type{sbx::models::material_feature::normal_map, "normal_map"},
    entry_type{sbx::models::material_feature::occlusion, "occlusion"},
    entry_type{sbx::models::material_feature::height, "height"},
    entry_type{sbx::models::material_feature::clearcoat, "clearcoat"},
    entry_type{sbx::models::material_feature::anisotropy, "anisotropy"},
  };

}; // struct sbx::utility::enum_mapping

#endif // LIBSBX_MODELS_STATIC_MESH_SUBRENDERER_HPP_
