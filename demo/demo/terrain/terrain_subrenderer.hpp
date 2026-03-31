// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_SUBRENDERER_HPP_
#define DEMO_TERRAIN_SUBRENDERER_HPP_

#include <vector>
#include <cstdint>

#include <libsbx/graphics/graphics.hpp>
#include <libsbx/scenes/scenes.hpp>

#include <libsbx/graphics/images/separate_image2d_array.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>

#include <demo/terrain/terrain_module.hpp>

namespace demo {

class terrain_subrenderer : public sbx::graphics::subrenderer {

  static constexpr auto clipmap_grid_size = 128u;
  static constexpr auto clipmap_grid_verts = clipmap_grid_size + 1u;
  static constexpr auto clipmap_ring_count = 5u;

  static constexpr auto terrain_layer_count = 6u;

  class pipeline : public sbx::graphics::graphics_pipeline {

    inline static const auto pipeline_definition = sbx::graphics::pipeline_definition{
      .depth = sbx::graphics::depth::read_write,
      .uses_transparency = false,
      .rasterization_state = sbx::graphics::rasterization_state{
        .polygon_mode = sbx::graphics::polygon_mode::fill,
        .cull_mode = sbx::graphics::cull_mode::back,
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

  terrain_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path);

  ~terrain_subrenderer() override = default;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override;

  auto set_tiling_scale(std::float_t scale) -> void {
    _tiling_scale = scale;
  }

private:

  auto _generate_grid_indices() -> std::vector<std::uint32_t>;

  pipeline _pipeline;

  sbx::graphics::push_handler _push_handler;
  sbx::graphics::descriptor_handler _descriptor_handler;

  sbx::graphics::storage_buffer_handle _index_buffer;

  std::array<sbx::graphics::image2d_handle, terrain_layer_count> _layer_textures;
  sbx::graphics::separate_image2d_array _terrain_images;
  sbx::graphics::sampler_state_handle _terrain_sampler;

  sbx::graphics::image2d_handle _noise_image;

  std::uint32_t _index_count{};
  std::float_t _tiling_scale{0.08f};

}; // class terrain_subrenderer

} // namespace demo

#endif // DEMO_TERRAIN_SUBRENDERER_HPP_
