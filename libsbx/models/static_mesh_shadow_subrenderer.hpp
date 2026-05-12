// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_STATIC_MESH_SHADOW_SUBRENDERER_HPP_
#define LIBSBX_MODELS_STATIC_MESH_SHADOW_SUBRENDERER_HPP_

#include <filesystem>
#include <unordered_map>
#include <array>

#include <vulkan/vulkan.h>

#include <libsbx/memory/tracking_allocator.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/models/models.hpp>
#include <libsbx/models/material_draw_list.hpp>
#include <libsbx/models/static_mesh_material_draw_list.hpp>

namespace sbx::models {

class static_mesh_shadow_subrenderer final : public graphics::subrenderer {

  inline static const auto pipeline_definition = graphics::pipeline_definition{
    .depth = graphics::depth::read_write,
    .uses_transparency = false,
    .rasterization_state = graphics::rasterization_state{
      .polygon_mode = graphics::polygon_mode::fill,
      .cull_mode = graphics::cull_mode::none,
      .front_face = graphics::front_face::counter_clockwise
    }
  };

  inline static constexpr auto default_pipeline_path = std::string_view{"engine://shaders/material_gbuffer"};

public:

  static_mesh_shadow_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& base_pipeline = default_pipeline_path);

  ~static_mesh_shadow_subrenderer() override;

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

  inline static const auto _entry_points = std::array<std::string, 3u>{
    "shadow_opaque_main", // alpha_mode::opaque
    "shadow_mask_main",   // alpha_mode::mask 
    "blend_main"          // alpha_mode::blend
  };

  std::vector<graphics::attachment_description> _attachments;
  std::filesystem::path _base_pipeline;

  inline static std::unordered_map<models::material_key, pipeline_data, models::material_key_hash> _pipeline_cache{};

  std::unordered_map<graphics::graphics_pipeline_handle, descriptor_data> _descriptor_cache{};

}; // class shadow_subrenderer

} // namespace sbx::shadows

#endif // LIBSBX_MODELS_STATIC_MESH_SHADOW_SUBRENDERER_HPP_
