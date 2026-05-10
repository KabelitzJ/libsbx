// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_BORDER_SUBRENDERER_HPP_
#define DEMO_TERRAIN_BORDER_SUBRENDERER_HPP_

#include <filesystem>
#include <vector>

#include <libsbx/graphics/graphics.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/scenes/scenes.hpp>

namespace demo {

class border_subrenderer : public sbx::graphics::subrenderer {

  class pipeline : public sbx::graphics::graphics_pipeline {

    inline static const auto pipeline_definition = sbx::graphics::pipeline_definition{
      .depth = sbx::graphics::depth::read_write,
      .uses_transparency = false,
      .rasterization_state = sbx::graphics::rasterization_state{
        .polygon_mode = sbx::graphics::polygon_mode::fill,
        .cull_mode = sbx::graphics::cull_mode::none,
        .front_face = sbx::graphics::front_face::counter_clockwise,
        .depth_bias = sbx::graphics::depth_bias{
          .constant_factor = -1.0f,
          .clamp = 0.0f,
          .slope_factor = -1.5f
        }
      }
    };

    using base = sbx::graphics::graphics_pipeline;

  public:

    pipeline(const std::filesystem::path& path, const std::vector<sbx::graphics::attachment_description>& attachments)
    : base{path, attachments, pipeline_definition} { }

    ~pipeline() override = default;

  }; // class pipeline

public:

  border_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path);
  ~border_subrenderer() override = default;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override;

  auto set_width(std::float_t width) -> void {
    _width = width;
  }

  auto set_color(const sbx::math::color& color) -> void {
    _color = color;
  }

  auto set_height_offset(std::float_t offset) -> void {
    _height_offset = offset;
  }

  auto set_subdivisions(std::uint32_t count) -> void {
    _subdivisions = std::max(count, 1u);
  }

private:

  pipeline _pipeline;
  sbx::graphics::push_handler _push_handler;
  sbx::graphics::descriptor_handler _descriptor_handler;

  std::float_t _width{0.30f};
  std::float_t _height_offset{0.08f};
  std::uint32_t _subdivisions{16u};
  sbx::math::color _color{0.05f, 0.04f, 0.03f};

}; // class border_subrenderer

} // namespace demo

#endif // DEMO_TERRAIN_BORDER_SUBRENDERER_HPP_
