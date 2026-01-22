// SPDX-License-Identifier: MIT
#ifndef LIBSBX_MODELS_FOLIAGE_SUBRENDERER_HPP_
#define LIBSBX_MODELS_FOLIAGE_SUBRENDERER_HPP_

#include <optional>

#include <libsbx/math/vector4.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/scenes/scenes_module.hpp>

namespace sbx::models {

class foliage_subrenderer final : public sbx::graphics::subrenderer {

  class pipeline : public sbx::graphics::graphics_pipeline {

    inline static const auto pipeline_definition = sbx::graphics::pipeline_definition{
      .depth = sbx::graphics::depth::read_write,
      .uses_transparency = false,
      .rasterization_state = sbx::graphics::rasterization_state{
        .polygon_mode = sbx::graphics::polygon_mode::fill,
        .cull_mode = sbx::graphics::cull_mode::none,
        .front_face = sbx::graphics::front_face::counter_clockwise
      },
      .primitive_topology = sbx::graphics::primitive_topology::triangle_list
    };
  
    using base_type = sbx::graphics::graphics_pipeline;
  
  public:
  
    pipeline(const std::filesystem::path& path, const std::vector<graphics::attachment_description>& attachments)
    : base_type{path, attachments, pipeline_definition} { }
  
    ~pipeline() override = default;
  
  }; // class pipeline

public:

  foliage_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path, const graphics::storage_buffer_handle& grass_buffer, const graphics::storage_buffer_handle& draw_command_buffer)
  : sbx::graphics::subrenderer{},
    _pipeline{path, attachments},
    _push_handler{_pipeline},
    _grass_buffer{grass_buffer},
    _draw_command_buffer{draw_command_buffer} {
    auto& settings = core::engine::settings(); 

    settings.set("foliage_task::green_top_r", 0.2f, 0.0f, 1.0f);
    settings.set("foliage_task::green_top_g", 0.5f, 0.0f, 1.0f);
    settings.set("foliage_task::green_top_b", 0.2f, 0.0f, 1.0f);
    settings.set("foliage_task::green_top_a", 1.0f, 0.0f, 1.0f);

    settings.set("foliage_task::green_bottom_r", 0.2f, 0.0f, 1.0f);
    settings.set("foliage_task::green_bottom_g", 0.5f, 0.0f, 1.0f);
    settings.set("foliage_task::green_bottom_b", 0.2f, 0.0f, 1.0f);
    settings.set("foliage_task::green_bottom_a", 1.0f, 0.0f, 1.0f);
  }

  ~foliage_subrenderer() override {

  }

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override {
    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

    auto& scene = scenes_module.scene();

    auto camera_node = scene.camera();

    auto& camera = scene.get_component<sbx::scenes::camera>(camera_node);

    const auto& projection = camera.projection();

    const auto& camera_global_transform = scene.get_component<sbx::scenes::global_transform>(camera_node);

    const auto view = sbx::math::matrix4x4::inverted(scene.world_transform(camera_node));

    auto& grass_buffer = graphics_module.get_resource<graphics::storage_buffer>(_grass_buffer);
    auto& draw_command_buffer = graphics_module.get_resource<graphics::storage_buffer>(_draw_command_buffer);

    // [TODO] : Implement color / vectors as settings
    auto& settings = core::engine::settings(); 

    auto green_bottom = math::color{
      *settings.get<std::float_t>("foliage_task::green_bottom_r"),
      *settings.get<std::float_t>("foliage_task::green_bottom_g"),
      *settings.get<std::float_t>("foliage_task::green_bottom_b"),
      *settings.get<std::float_t>("foliage_task::green_bottom_a")
    };

    auto green_top = math::color{
      *settings.get<std::float_t>("foliage_task::green_top_r"),
      *settings.get<std::float_t>("foliage_task::green_top_g"),
      *settings.get<std::float_t>("foliage_task::green_top_b"),
      *settings.get<std::float_t>("foliage_task::green_top_a")
    };
    
    _pipeline.bind(command_buffer);

    _push_handler.push("blades", grass_buffer.address());
    _push_handler.push("view_projection", projection * view);
    _push_handler.push("global_time", core::engine::time().value());
    _push_handler.push("green_bottom", green_bottom);
    _push_handler.push("green_top", green_top);

    _push_handler.bind(command_buffer);

    command_buffer.draw_indirect(draw_command_buffer, 0, 1);
  }

private:

  pipeline _pipeline;

  sbx::graphics::push_handler _push_handler;

  graphics::storage_buffer_handle _grass_buffer;
  graphics::storage_buffer_handle _draw_command_buffer;

}; // class debug_subrenderer

} // namespace sbx::models

#endif // LIBSBX_MODELS_FOLIAGE_SUBRENDERER_HPP_
