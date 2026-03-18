// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_SUBRENDERER_HPP_
#define DEMO_TERRAIN_SUBRENDERER_HPP_

#include <vector>
#include <cstdint>

#include <libsbx/graphics/graphics.hpp>
#include <libsbx/scenes/scenes.hpp>

#include <demo/terrain/terrain_module.hpp>

namespace demo {

class terrain_subrenderer : public sbx::graphics::subrenderer {

  static constexpr auto clipmap_grid_size = 64u;
  static constexpr auto clipmap_grid_verts = clipmap_grid_size + 1u;
  static constexpr auto clipmap_ring_count = 5u;

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

private:

  auto _generate_grid_indices() -> std::vector<std::uint32_t>;

  pipeline _pipeline;

  sbx::graphics::push_handler _push_handler;
  sbx::graphics::descriptor_handler _descriptor_handler;

  sbx::graphics::storage_buffer_handle _index_buffer;
  sbx::graphics::storage_buffer_handle _height_buffer;
  sbx::graphics::storage_buffer_handle _splat_buffer;

  std::uint32_t _index_count{};

}; // class terrain_subrenderer

} // namespace demo

#endif // DEMO_TERRAIN_SUBRENDERER_HPP_
