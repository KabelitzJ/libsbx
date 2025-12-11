#ifndef LIBSBX_SHADOWS_SHADOW_SUBRENDERER_HPP_
#define LIBSBX_SHADOWS_SHADOW_SUBRENDERER_HPP_

#include <unordered_map>
#include <algorithm>

#include <libsbx/utility/fast_mod.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/devices/devices_module.hpp>
#include <libsbx/devices/window.hpp>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/graphics/images/image.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>
#include <libsbx/graphics/images/separate_image2d_array.hpp>
#include <libsbx/graphics/images/depth_image.hpp>

#include <libsbx/graphics/buffers/push_handler.hpp>

#include <libsbx/graphics/buffers/uniform_handler.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>
#include <libsbx/scenes/components/id.hpp>
#include <libsbx/scenes/components/directional_light.hpp>
#include <libsbx/scenes/components/tag.hpp>

#include <libsbx/models/mesh.hpp>
#include <libsbx/models/vertex3d.hpp>
#include <libsbx/models/material.hpp>
#include <libsbx/models/material_draw_list.hpp>

namespace sbx::shadows {

class shadow_subrenderer : public graphics::subrenderer {

  class pipeline : public graphics::graphics_pipeline {

    inline static const auto pipeline_definition = graphics::pipeline_definition{
      .depth = graphics::depth::read_write,
      .uses_transparency = false,
      .rasterization_state = graphics::rasterization_state{
        .polygon_mode = graphics::polygon_mode::fill,
        .cull_mode = graphics::cull_mode::front,
        .front_face = graphics::front_face::counter_clockwise
      }
    };

    using base = graphics::graphics_pipeline;

  public:

    pipeline(const std::filesystem::path& path, const std::vector<graphics::attachment_description>& attachments)
    : base{path, attachments, pipeline_definition} { }

    ~pipeline() override = default;

  }; // class pipeline

public:

  shadow_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path)
  : graphics::subrenderer{},
    _pipeline{path, attachments},
    _push_handler{_pipeline},
    _scene_descriptor_handler{_pipeline, 0u} { }

  ~shadow_subrenderer() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    SBX_PROFILE_SCOPE("shadow_subrenderer::render");

    auto& assets_module = core::engine::get_module<assets::assets_module>();
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

    auto& scene = scenes_module.scene();

    _scene_uniform_handler.push("light_space", scene.light_space());

    _scene_uniform_handler.push("time", std::fmod(core::engine::time().value() * 0.5f, 1.0f));

    // auto& draw_list = pass().draw_list<models::static_mesh_draw_list>("static_mesh");

    // _pipeline.bind(command_buffer);

    // _scene_descriptor_handler.push("scene", _scene_uniform_handler);
    
    // if (!_scene_descriptor_handler.update(_pipeline)) {
    //   return;
    // }

    // _scene_descriptor_handler.bind_descriptors(command_buffer);

    // _push_handler.push("transform_data_buffer", draw_list.buffer(models::static_mesh_draw_list::transform_data_buffer_name).address());
    // _push_handler.push("instance_data_buffer", draw_list.buffer(models::static_mesh_draw_list::opaque_instance_data_buffer_name).address());

    // for (const auto& [mesh_id, range] : draw_list.draw_ranges("opaque")) {
    //   auto& mesh = assets_module.get_asset<models::mesh>(mesh_id);
      
    //   mesh.bind(command_buffer);
      
    //   _push_handler.push("vertex_buffer", mesh.address());

    //   _push_handler.bind(command_buffer);

    //   command_buffer.draw_indexed_indirect(draw_list.buffer(models::static_mesh_draw_list::opaque_draw_commands_buffer_name), range.offset, range.count);
    // }
  }

private:

  pipeline _pipeline;

  graphics::push_handler _push_handler;
  graphics::descriptor_handler _scene_descriptor_handler;
  graphics::uniform_handler _scene_uniform_handler;

}; // class shadow_subrenderer

} // namespace sbx::shadows

#endif // LIBSBX_SHADOWS_SHADOW_SUBRENDERER_HPP_
