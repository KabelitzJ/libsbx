// SPDX-License-Identifier: MIT
#include <demo/building/road_subrenderer.hpp>

#include <libsbx/scenes/scenes.hpp>
#include <libsbx/graphics/graphics.hpp>

#include <demo/building/building_module.hpp>

namespace demo {

road_subrenderer::road_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path)
: sbx::graphics::subrenderer{},
  _pipeline{path, attachments},
  _push_handler{_pipeline},
  _descriptor_handler{_pipeline, 0u} { }

auto road_subrenderer::mark_chunk_dirty(chunk_coord chunk_coordinates) -> void {
  auto entry = _gpu_chunks.find(chunk_coordinates);

  if (entry != _gpu_chunks.end()) {
    entry->second.is_uploaded = false;
  }
}

auto road_subrenderer::mark_chunks_dirty(const std::unordered_set<chunk_coord, chunk_coord_hash>& dirty_chunks) -> void {
  for (const auto& chunk_coordinates : dirty_chunks) {
    mark_chunk_dirty(chunk_coordinates);
  }
}

auto road_subrenderer::_ensure_gpu_chunk(chunk_coord chunk_coordinates) -> gpu_road_chunk& {
  auto entry = _gpu_chunks.find(chunk_coordinates);

  if (entry != _gpu_chunks.end()) {
    return entry->second;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& data = _gpu_chunks[chunk_coordinates];

  data.vertex_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  data.index_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  data.index_count = 0;
  data.is_uploaded = false;

  return data;
}

auto road_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  EASY_BLOCK("road_subrenderer::render", profiler::colors::Blue);
  SBX_PROFILE_SCOPE("road_subrenderer::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();
  auto& building_module = sbx::core::engine::get_module<demo::building_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();

  if (building_module.roads_dirty()) {
    for (auto& [chunk_coordinates, gpu_chunk] : _gpu_chunks) {
      gpu_chunk.is_uploaded = false;
    }

    terrain_module.grid().for_each_chunk([&](const chunk_coord& chunk_coordinates, const grid_chunk& chunk) {
      auto has_roads_in_chunk = false;

      for (auto local_y = 0u; local_y < chunk_size && !has_roads_in_chunk; ++local_y) {
        for (auto local_x = 0u; local_x < chunk_size && !has_roads_in_chunk; ++local_x) {
          if (chunk.at(local_x, local_y).road_type != 0) {
            has_roads_in_chunk = true;
          }
        }
      }

      if (has_roads_in_chunk) {
        _ensure_gpu_chunk(chunk_coordinates);
      }
    });

    building_module.clear_roads_dirty();
  }

  auto has_any_roads = false;

  for (auto& [chunk_coordinates, gpu_chunk] : _gpu_chunks) {
    if (!gpu_chunk.is_uploaded) {
      auto mesh = build_road_chunk_mesh(terrain_module.grid(), terrain_module.heightmap(), chunk_coordinates);

      if (mesh.is_empty) {
        gpu_chunk.index_count = 0;
        gpu_chunk.is_uploaded = true;
        continue;
      }

      auto& vertex_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(gpu_chunk.vertex_buffer);
      _update_buffer(vertex_buffer, mesh.vertices);

      auto& index_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(gpu_chunk.index_buffer);
      _update_buffer(index_buffer, mesh.indices);

      gpu_chunk.index_count = static_cast<std::uint32_t>(mesh.indices.size());
      gpu_chunk.is_uploaded = true;
    }

    if (gpu_chunk.index_count > 0) {
      has_any_roads = true;
    }
  }

  if (!has_any_roads) {
    return;
  }

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", environment.uniform_handler());

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  for (auto& [chunk_coordinates, gpu_chunk] : _gpu_chunks) {
    if (gpu_chunk.index_count == 0) {
      continue;
    }

    auto& vertex_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(gpu_chunk.vertex_buffer);
    auto& index_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(gpu_chunk.index_buffer);

    _push_handler.push("vertex_data_buffer", vertex_buffer.address());
    _push_handler.bind(command_buffer);

    command_buffer.bind_index_buffer(index_buffer.handle(), 0, VK_INDEX_TYPE_UINT32);
    command_buffer.draw_indexed(gpu_chunk.index_count, 1, 0, 0, 0);
  }
}

} // namespace demo
