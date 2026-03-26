// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDING_ROAD_SUBRENDERER_HPP_
#define DEMO_BUILDING_ROAD_SUBRENDERER_HPP_

#include <vector>
#include <cstdint>

#include <libsbx/graphics/graphics.hpp>
#include <libsbx/scenes/scenes.hpp>

#include <libsbx/graphics/images/separate_image2d_array.hpp>
#include <libsbx/graphics/images/sampler_state.hpp>

#include <demo/terrain/terrain_module.hpp>

#include <demo/building/road_types.hpp>
#include <demo/building/road_mesh.hpp>

namespace demo {

class road_subrenderer : public sbx::graphics::subrenderer {

  class pipeline : public sbx::graphics::graphics_pipeline {

    inline static const auto pipeline_definition = sbx::graphics::pipeline_definition{
      .depth = sbx::graphics::depth::read_write,
      .uses_transparency = false,
      .rasterization_state = sbx::graphics::rasterization_state{
        .polygon_mode = sbx::graphics::polygon_mode::fill,
        .cull_mode = sbx::graphics::cull_mode::none,
        .front_face = sbx::graphics::front_face::counter_clockwise,
        .depth_bias = sbx::graphics::depth_bias{
          .constant_factor = -2.0f,
          .slope_factor = -2.0f
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

  road_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path);

  ~road_subrenderer() override = default;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override;

  auto mark_dirty() -> void;

private:

  template<typename Type>
  auto _update_buffer(sbx::graphics::storage_buffer& buffer, const std::vector<Type>& data) -> void {
    auto required_size = static_cast<VkDeviceSize>(data.size() * sizeof(Type));

    if (required_size > buffer.size()) {
      buffer.resize(static_cast<VkDeviceSize>(required_size * 1.5f));
    }

    buffer.update(data.data(), data.size() * sizeof(Type));
  }

  pipeline _pipeline;

  sbx::graphics::push_handler _push_handler;
  sbx::graphics::descriptor_handler _descriptor_handler;

  sbx::graphics::storage_buffer_handle _vertex_buffer;
  sbx::graphics::storage_buffer_handle _index_buffer;
  std::uint32_t _index_count{0};
  bool _is_dirty{true};

  static constexpr auto road_texture_count = 4u;
  static constexpr auto road_image_count = road_texture_count * 2u;

  std::array<sbx::graphics::image2d_handle, road_image_count> _road_texture_handles;
  sbx::graphics::separate_image2d_array _road_images;
  sbx::graphics::sampler_state_handle _road_sampler;

}; // class road_subrenderer

} // namespace demo

#endif // DEMO_BUILDING_ROAD_SUBRENDERER_HPP_
