// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDING_ROAD_SUBRENDERER_HPP_
#define DEMO_BUILDING_ROAD_SUBRENDERER_HPP_

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>

#include <libsbx/graphics/graphics.hpp>
#include <libsbx/scenes/scenes.hpp>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/terrain_module.hpp>

#include <demo/building/road_types.hpp>
#include <demo/building/road_mesh.hpp>
#include <demo/building/road_placement.hpp>

namespace demo {

class road_subrenderer : public sbx::graphics::subrenderer {

  class pipeline : public sbx::graphics::graphics_pipeline {

    inline static const auto pipeline_definition = sbx::graphics::pipeline_definition{
      .depth = sbx::graphics::depth::read_write,
      .uses_transparency = false,
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

  struct gpu_road_chunk {
    sbx::graphics::storage_buffer_handle vertex_buffer;
    sbx::graphics::storage_buffer_handle index_buffer;
    std::uint32_t index_count{0};
    bool is_uploaded{false};
  }; // struct gpu_road_chunk

public:

  road_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path);

  ~road_subrenderer() override = default;

  auto render(sbx::graphics::command_buffer& command_buffer) -> void override;

  auto mark_chunk_dirty(chunk_coord chunk_coordinates) -> void;

  auto mark_chunks_dirty(const std::unordered_set<chunk_coord, chunk_coord_hash>& dirty_chunks) -> void;

private:

  auto _ensure_gpu_chunk(chunk_coord chunk_coordinates) -> gpu_road_chunk&;

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

  std::unordered_map<chunk_coord, gpu_road_chunk, chunk_coord_hash> _gpu_chunks;

}; // class road_subrenderer

} // namespace demo

#endif // DEMO_BUILDING_ROAD_SUBRENDERER_HPP_
