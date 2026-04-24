// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_STATIC_MESH_MATERIAL_SUBRENDERER_HPP_
#define LIBSBX_MODELS_STATIC_MESH_MATERIAL_SUBRENDERER_HPP_

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

#include <libsbx/reflection/description.hpp>

#include <libsbx/memory/tracking_allocator.hpp>

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
#include <libsbx/models/static_mesh_material_draw_list.hpp>
#include <libsbx/models/frustum_culling_task.hpp>

namespace sbx::models {

class static_mesh_material_subrenderer final : public graphics::subrenderer {

  inline static const auto pipeline_definition = graphics::pipeline_definition{
    .depth = graphics::depth::read_write,
    .uses_transparency = false,
    .rasterization_state = graphics::rasterization_state{
      .polygon_mode = graphics::polygon_mode::fill,
      .cull_mode = graphics::cull_mode::back,
      .front_face = graphics::front_face::counter_clockwise
    }
  };

  inline static constexpr auto default_pipeline_path = std::string_view{"engine://shaders/deferred_pbr_material"};

public:

  static_mesh_material_subrenderer(const std::vector<graphics::attachment_description>& attachments, const static_mesh_material_draw_list::bucket bucket, const std::filesystem::path& base_pipeline = default_pipeline_path);

  ~static_mesh_material_subrenderer() override;

  auto render(graphics::command_buffer& command_buffer) -> void override;

private:

  struct pipeline_data {

    graphics::graphics_pipeline_handle pipeline;
    graphics::push_handler push_handler;

    pipeline_data(const graphics::graphics_pipeline_handle& handle)
    : pipeline{handle},
      push_handler{pipeline} { }

  }; // struct pipeline_data

  struct descriptor_data {

    graphics::descriptor_handler scene_descriptor_handler;
    graphics::descriptor_handler sampler_descriptor_handler;
    graphics::descriptor_handler image_descriptor_handler;

    descriptor_data(const graphics::graphics_pipeline_handle& handle)
    : scene_descriptor_handler{handle, 0u},
      sampler_descriptor_handler{handle, 1u},
      image_descriptor_handler{handle, 2u} { }

  }; // struct descriptor_data

  auto _get_or_create_pipeline(const material_key& key) -> pipeline_data&;

  auto _get_or_create_descriptor_data(const graphics::graphics_pipeline_handle& handle) -> descriptor_data&;

  inline static const auto _entry_point = std::array<std::string, 3u>{
    "opaque_main",  // alpha_mode::opaque
    "mask_main",    // alpha_mode::mask 
    "blend_main"    // alpha_mode::blend
  };

  std::vector<graphics::attachment_description> _attachments;
  std::filesystem::path _base_pipeline;
  static_mesh_material_draw_list::bucket _bucket;

  inline static auto _pipeline_cache = std::unordered_map<material_key, pipeline_data, material_key_hash>{};

  std::unordered_map<graphics::graphics_pipeline_handle, descriptor_data> _descriptor_cache{};

}; // class static_mesh_subrenderer

} // namespace sbx::models

template<>
struct sbx::reflection::description<sbx::models::material_feature> {

  static constexpr auto name() -> std::string_view {
    return "material_feature";
  }

  static constexpr auto enumerators() {
    return std::make_tuple(
      enumerator{"cast_shadow", sbx::models::material_feature::cast_shadow},
      enumerator{"receive_shadow", sbx::models::material_feature::receive_shadow},
      enumerator{"invert_backface_normals", sbx::models::material_feature::invert_backface_normals}
    );
  }

}; // struct sbx::reflection::description<sbx::models::material_feature>

#endif // LIBSBX_MODELS_STATIC_MESH_MATERIAL_SUBRENDERER_HPP_
