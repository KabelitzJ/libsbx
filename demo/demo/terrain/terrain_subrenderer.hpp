// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_TERRAIN_SUBRENDERER_HPP_
#define DEMO_TERRAIN_TERRAIN_SUBRENDERER_HPP_

#include <unordered_map>
#include <vector>
#include <cstdint>

#include <libsbx/math/vector3.hpp>

#include <libsbx/graphics/graphics.hpp>

#include <libsbx/scenes/scenes.hpp>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/terrain_module.hpp>

namespace demo {

class terrain_subrenderer : public sbx::graphics::subrenderer {

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

    pipeline(const std::filesystem::path& shader_path, const std::vector<sbx::graphics::attachment_description>& attachments)
    : base{shader_path, attachments, pipeline_definition} { }

    ~pipeline() override = default;

  }; // class pipeline

  struct gpu_chunk_data {
    sbx::graphics::storage_buffer_handle height_buffer;
    bool uploaded{false};
  }; // struct gpu_chunk_data

public:

  terrain_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& shader_path);

  ~terrain_subrenderer() override = default;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override;

  auto set_view_distance(std::float_t view_distance) -> void {
    _view_distance = view_distance;
  }

private:

  auto _generate_chunk_indices() -> std::vector<std::uint32_t>;

  auto _ensure_gpu_chunk(chunk_coord chunk_coordinates) -> gpu_chunk_data&;

  auto _upload_chunk(chunk_coord chunk_coordinates, gpu_chunk_data& gpu_chunk, const height_chunk& height_chunk_data) -> void;

  template<typename Type>
  auto _update_buffer(sbx::graphics::storage_buffer& buffer, std::span<const Type> elements) -> void {
    const auto required_size = static_cast<VkDeviceSize>(elements.size() * sizeof(Type));

    if (required_size > buffer.size()) {
      buffer.resize(static_cast<VkDeviceSize>(required_size * 1.5f));
    }

    buffer.update(elements.data(), elements.size() * sizeof(Type));
  }

  pipeline _pipeline;

  sbx::graphics::push_handler _push_handler;
  sbx::graphics::descriptor_handler _descriptor_handler;

  sbx::graphics::storage_buffer_handle _index_buffer;
  std::uint32_t _index_count{};

  std::unordered_map<chunk_coord, gpu_chunk_data, chunk_coord_hash> _gpu_chunks;

  std::float_t _view_distance{500.0f};

}; // class terrain_subrenderer

} // namespace demo

#endif // DEMO_TERRAIN_TERRAIN_SUBRENDERER_HPP_
