// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_SKYBOX_SUBRENDERER_HPP_
#define LIBSBX_SCENES_SKYBOX_SUBRENDERER_HPP_

#include <libsbx/graphics/subrenderer.hpp>

#include <libsbx/graphics/images/cube_image.hpp>

#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/pipeline/vertex_input_description.hpp>
#include <libsbx/graphics/pipeline/mesh.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

#include <libsbx/graphics/buffers/uniform_handler.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/components/skybox.hpp>
#include <libsbx/scenes/components/camera.hpp>

namespace sbx::scenes {

struct vertex3d {
  math::vector3 position;
}; // struct vertex3d

class mesh : public graphics::mesh<vertex3d> {

public:

  mesh(std::vector<vertex3d>&& vertices, std::vector<std::uint32_t>&& indices)
  : graphics::mesh<vertex3d>{std::move(vertices), std::move(indices)} { }

  ~mesh() override = default;
  
}; // class mesh

class skybox_subrenderer : public sbx::graphics::subrenderer {

  inline static const auto pipeline_definition = graphics::pipeline_definition{
    .depth = graphics::depth::read_only,
    .compare_operation = graphics::compare_operation::less_or_equal,
    .uses_transparency = false,
    .rasterization_state = graphics::rasterization_state{
      .polygon_mode = graphics::polygon_mode::fill,
      .cull_mode = graphics::cull_mode::none,
      .front_face = graphics::front_face::counter_clockwise
    }
  };

  inline static constexpr auto default_shader_path = std::string_view{"engine://shaders/skybox"};

public:

  skybox_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path = default_shader_path)
  : graphics::subrenderer{},
    _pipeline{path, attachments, pipeline_definition},
    _descriptor_handler{_pipeline, 0u},
    _push_handler{_pipeline} { }

  ~skybox_subrenderer() override = default;

  auto render(graphics::command_buffer& command_buffer) -> void override {
    EASY_BLOCK("skybox_subrenderer::render", profiler::colors::Indigo500);

    auto& assets_module = core::engine::get_module<assets::assets_module>();
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

    auto& scene = scenes_module.active_scene();
    auto& environment = scene.environment();
    auto& graph = scene.graph();

    const auto camera_node = environment.camera();
    
    if (!graph.has_component<scenes::skybox>(camera_node)) {
      utility::logger<"scenes">::warn("Skybox subrenderer: No camera node with skybox component found");
      return;
    }

    const auto& skybox = graph.get_component<scenes::skybox>(camera_node);

    _pipeline.bind(command_buffer);

    _descriptor_handler.push("scene", environment.uniform_handler());
    _descriptor_handler.push("skybox", graphics_module.get_resource<graphics::cube_image>(skybox.cube_image));

    _push_handler.push("tint", skybox.tint);

    if (!_descriptor_handler.update(_pipeline)) {
      return;
    }

    _descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw(3u, 1u, 0u, 0u);
  }

private:

  graphics::graphics_pipeline _pipeline;

  graphics::descriptor_handler _descriptor_handler;

  graphics::push_handler _push_handler;

}; // class skybox_subrenderer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_SKYBOX_SUBRENDERER_HPP_
