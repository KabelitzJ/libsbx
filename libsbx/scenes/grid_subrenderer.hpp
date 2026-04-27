// SPDX-License-Identifier: MIT
#ifndef LIBSBX_SCENES_GRID_SUBRENDERER_HPP_
#define LIBSBX_SCENES_GRID_SUBRENDERER_HPP_

#include <libsbx/math/vector4.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/subrenderer.hpp>
#include <libsbx/graphics/pipeline/graphics_pipeline.hpp>
#include <libsbx/graphics/buffers/push_handler.hpp>
#include <libsbx/graphics/buffers/storage_handler.hpp>
#include <libsbx/graphics/descriptor/descriptor_handler.hpp>

namespace sbx::scenes {

class grid_subrenderer final : public sbx::graphics::subrenderer {

  inline static const auto pipeline_definition = sbx::graphics::pipeline_definition{
    .depth = sbx::graphics::depth::read_only,
    .uses_transparency = true,
    .rasterization_state = sbx::graphics::rasterization_state{
      .polygon_mode = sbx::graphics::polygon_mode::fill,
      .cull_mode = sbx::graphics::cull_mode::none,
      .front_face = sbx::graphics::front_face::counter_clockwise
    }
  };

  inline static constexpr auto default_shader_path = std::string_view{"engine://shaders/grid"};

public:

  grid_subrenderer(const std::vector<graphics::attachment_description>& attachments, const std::filesystem::path& path = default_shader_path)
  : sbx::graphics::subrenderer{},
    _pipeline{path, attachments, pipeline_definition},
    _push_handler{_pipeline},
    _descriptor_handler{_pipeline, 0u} { }

  ~grid_subrenderer() override {

  }

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override {
    EASY_BLOCK("grid_subrenderer::render");

    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
    auto& scene = scenes_module.active_scene();
    auto& environment = scene.environment();
    auto& graph = scene.graph();
    
    _pipeline.bind(command_buffer);

    _push_handler.push("origin", sbx::math::vector4::zero);

    _descriptor_handler.push("scene", environment.uniform_handler());

    if (!_descriptor_handler.update(_pipeline)) {
      return;
    }

    _descriptor_handler.bind_descriptors(command_buffer);
    _push_handler.bind(command_buffer);

    command_buffer.draw(6u, 1u, 0u, 0u);
  }

private:

  graphics::graphics_pipeline _pipeline;

  sbx::graphics::push_handler _push_handler;

  sbx::graphics::descriptor_handler _descriptor_handler;

}; // class debug_subrenderer

} // namespace sbx::scenes

#endif // LIBSBX_SCENES_GRID_SUBRENDERER_HPP_
