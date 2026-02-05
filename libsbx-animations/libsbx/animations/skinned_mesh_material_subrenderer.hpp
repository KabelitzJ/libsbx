// SPDX-License-Identifier: MIT
#ifndef LIBSBX_ANIMATIONS_SKINNED_MESH_MATERIAL_SUBRENDERER_HPP_
#define LIBSBX_ANIMATIONS_SKINNED_MESH_MATERIAL_SUBRENDERER_HPP_

#include <filesystem>
#include <unordered_set>
#include <ranges>
#include <algorithm>
#include <iterator>

#include <easy/profiler.h>

#include <range/v3/view/enumerate.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/pipeline/pipeline.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/scenes/components/skinned_mesh.hpp>

#include <libsbx/models/material_draw_list.hpp>

#include <libsbx/animations/mesh.hpp>
#include <libsbx/animations/animator.hpp>

#include <libsbx/animations/skinned_mesh_material_draw_list.hpp>
#include <libsbx/animations/skinning_task.hpp>

namespace sbx::animations {

class skinned_mesh_material_subrenderer final : public graphics::subrenderer {

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

  skinned_mesh_material_subrenderer(const memory::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline, const skinned_mesh_material_draw_list::bucket bucket, memory::observer_ptr<const skinning_task> skinning_task);

  ~skinned_mesh_material_subrenderer() override;

  auto render(graphics::command_buffer& command_buffer) -> void override;

private:

  struct pipeline_data {

    graphics::graphics_pipeline_handle pipeline;
    graphics::push_handler push_handler;

    pipeline_data(const graphics::graphics_pipeline_handle& handle);

  }; // struct pipeline_data

  struct descriptor_data {

    graphics::descriptor_handler scene_descriptor_handler;
    graphics::descriptor_handler sampler_descriptor_handler;
    graphics::descriptor_handler image_descriptor_handler;

    descriptor_data(const graphics::graphics_pipeline_handle& handle);

  }; // struct descriptor_data

  auto _get_or_create_pipeline(const models::material_key& key) -> pipeline_data&;

  auto _get_or_create_descriptor_data(const graphics::graphics_pipeline_handle& handle) -> descriptor_data&;

  inline static const auto _fs_entry = std::array<std::string, 3u>{
    "opaque_main",
    "mask_main",
    "blend_main"
  };

  memory::vector<graphics::attachment_description> _attachments;
  std::filesystem::path _base_pipeline;
  skinned_mesh_material_draw_list::bucket _bucket;

  memory::observer_ptr<const skinning_task> _skinning_task;

  inline static std::unordered_map<models::material_key, pipeline_data, models::material_key_hash> _pipeline_cache{};

  std::unordered_map<graphics::graphics_pipeline_handle, descriptor_data> _descriptor_cache{};

}; // class skinned_mesh_material_subrenderer

} // namespace sbx::animations

#endif // LIBSBX_ANIMATIONS_SKINNED_MESH_MATERIAL_SUBRENDERER_HPP_
