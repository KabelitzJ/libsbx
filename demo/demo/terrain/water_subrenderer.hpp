// SPDX-License-Identifier: MIT
#ifndef DEMO_WATER_WATER_SUBRENDERER_HPP_
#define DEMO_WATER_WATER_SUBRENDERER_HPP_

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <vector>

#include <libsbx/math/vector3.hpp>
#include <libsbx/math/color.hpp>

#include <libsbx/graphics/graphics.hpp>

#include <libsbx/scenes/scenes.hpp>

namespace demo {

class water_subrenderer : public sbx::graphics::subrenderer {

  static constexpr auto water_grid_size = 128u;
  static constexpr auto water_grid_verts = water_grid_size + 1u;

  class pipeline : public sbx::graphics::graphics_pipeline {

    inline static const auto pipeline_definition = sbx::graphics::pipeline_definition{
      .depth = sbx::graphics::depth::read_only,
      .uses_transparency = true,
      .rasterization_state = sbx::graphics::rasterization_state{
        .polygon_mode = sbx::graphics::polygon_mode::fill,
        .cull_mode = sbx::graphics::cull_mode::none,
        .front_face = sbx::graphics::front_face::counter_clockwise
      }
    };

    using base = sbx::graphics::graphics_pipeline;

  public:

    pipeline(const std::filesystem::path& path, const std::vector<sbx::graphics::attachment_description>& attachments)
    : base{path, attachments, pipeline_definition} { }

    ~pipeline() override = default;

  }; // class pipeline

public:

  water_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path);
  ~water_subrenderer() override = default;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override;

  auto set_water_level(std::float_t level) -> void {
    _water_level = level;
  }

  auto set_water_color_shallow(const sbx::math::color& color) -> void {
    _water_color_shallow = color;
  }

  auto set_water_color_deep(const sbx::math::color& color) -> void {
    _water_color_deep = color;
  }

  auto set_depth_fade_distance(std::float_t distance) -> void {
    _depth_fade_distance = distance;
  }

  auto set_roughness(std::float_t roughness) -> void {
    _roughness = roughness;
  }

  auto set_wave_amplitude(std::float_t amplitude) -> void {
    _wave_amplitude = amplitude;
  }

  auto set_wave_frequency(std::float_t frequency) -> void {
    _wave_frequency = frequency;
  }

  auto set_wave_speed(std::float_t speed) -> void {
    _wave_speed = speed;
  }

private:

  auto _generate_grid_indices() -> std::vector<std::uint32_t>;

  pipeline _pipeline;

  sbx::graphics::push_handler _push_handler;
  sbx::graphics::descriptor_handler _descriptor_handler;

  sbx::graphics::storage_buffer_handle _index_buffer;
  std::uint32_t _index_count{};

  std::float_t _water_level{1.2f};
  sbx::math::color _water_color_shallow{0.20f, 0.45f, 0.55f};
  sbx::math::color _water_color_deep{0.02f, 0.10f, 0.20f};
  std::float_t _depth_fade_distance{2.0f};
  std::float_t _roughness{0.04f};
  std::float_t _wave_amplitude{0.04f};
  std::float_t _wave_frequency{0.6f};
  std::float_t _wave_speed{0.9f};

}; // class water_subrenderer

} // namespace demo

#endif // DEMO_WATER_WATER_SUBRENDERER_HPP_
