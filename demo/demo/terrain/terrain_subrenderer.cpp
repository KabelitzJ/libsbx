// SPDX-License-Identifier: MIT
#include <demo/terrain/terrain_subrenderer.hpp>

#include <libsbx/scenes/scenes.hpp>

#include <libsbx/graphics/graphics.hpp>

namespace demo {

terrain_subrenderer::terrain_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& shader_path)
: sbx::graphics::subrenderer{},
  _pipeline{shader_path, attachments},
  _push_handler{_pipeline},
  _descriptor_handler{_pipeline, 0u} {

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto index_data = _generate_chunk_indices();
  _index_count = static_cast<std::uint32_t>(index_data.size());

  _index_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  auto& index_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);

  _update_buffer(index_buffer, std::span<const std::uint32_t>{index_data});
}

auto terrain_subrenderer::_generate_chunk_indices() -> std::vector<std::uint32_t> {
  auto index_data = std::vector<std::uint32_t>{};
  index_data.reserve(chunk_size * chunk_size * 6);

  for (auto cell_y = 0u; cell_y < chunk_size; ++cell_y) {
    for (auto cell_x = 0u; cell_x < chunk_size; ++cell_x) {
      auto top_left     = cell_y * chunk_vertices + cell_x;
      auto top_right    = top_left + 1;
      auto bottom_left  = (cell_y + 1) * chunk_vertices + cell_x;
      auto bottom_right = bottom_left + 1;

      index_data.push_back(top_left);
      index_data.push_back(bottom_left);
      index_data.push_back(top_right);

      index_data.push_back(top_right);
      index_data.push_back(bottom_left);
      index_data.push_back(bottom_right);
    }
  }

  return index_data;
}

auto terrain_subrenderer::_ensure_gpu_chunk(chunk_coord chunk_coordinates) -> gpu_chunk_data& {
  auto existing_chunk = _gpu_chunks.find(chunk_coordinates);

  if (existing_chunk != _gpu_chunks.end()) {
    return existing_chunk->second;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& gpu_chunk = _gpu_chunks[chunk_coordinates];

  gpu_chunk.height_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  gpu_chunk.uploaded = false;

  return gpu_chunk;
}

auto terrain_subrenderer::_upload_chunk(chunk_coord chunk_coordinates, gpu_chunk_data& gpu_chunk, const height_chunk& height_chunk_data) -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& height_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(gpu_chunk.height_buffer);

  _update_buffer(height_buffer, std::span<const std::float_t>{height_chunk_data.heights});

  gpu_chunk.uploaded = true;
}

auto terrain_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("terrain_subrenderer::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  auto& terrain_module_instance = sbx::core::engine::get_module<demo::terrain_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& scene_graph = scene.graph();

  auto camera_node = environment.camera();
  auto camera_position = scene_graph.world_position(camera_node);

  auto visible_chunks = terrain_module_instance.get_visible_chunks(camera_position.x(), camera_position.z(), _view_distance);

  if (visible_chunks.empty()) {
    return;
  }

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", environment.uniform_handler());

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  auto& index_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);

  command_buffer.bind_index_buffer(index_buffer.handle(), 0, VK_INDEX_TYPE_UINT32);

  auto cell_size = terrain_module_instance.cell_size();
  auto world_offset_x = terrain_module_instance.offset_x();
  auto world_offset_z = terrain_module_instance.offset_z();

  auto inverse_chunk_vertex_count = 1.0f / static_cast<std::float_t>(chunk_vertices);

  for (const auto& chunk_coordinates : visible_chunks) {
    terrain_module_instance.heightmap().ensure_chunk(chunk_coordinates);

    auto* height_chunk_data = terrain_module_instance.heightmap().get_chunk(chunk_coordinates);

    if (!height_chunk_data) {
      continue;
    }

    auto& gpu_chunk = _ensure_gpu_chunk(chunk_coordinates);

    if (!gpu_chunk.uploaded || height_chunk_data->is_dirty) {
      _upload_chunk(chunk_coordinates, gpu_chunk, *height_chunk_data);
      height_chunk_data->is_dirty = false;
    }

    auto& height_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(gpu_chunk.height_buffer);

    auto chunk_world_x = static_cast<std::float_t>(chunk_coordinates.x * static_cast<std::int32_t>(chunk_size)) * cell_size - world_offset_x;
    auto chunk_world_z = static_cast<std::float_t>(chunk_coordinates.y * static_cast<std::int32_t>(chunk_size)) * cell_size - world_offset_z;

    _push_handler.push("height_data_buffer", height_buffer.address());
    _push_handler.push("chunk_vertices", chunk_vertices);
    _push_handler.push("cell_size", cell_size);
    _push_handler.push("chunk_world_x", chunk_world_x);
    _push_handler.push("chunk_world_z", chunk_world_z);
    _push_handler.push("inv_chunk_vertices", inverse_chunk_vertex_count);
    _push_handler.bind(command_buffer);

    command_buffer.draw_indexed(_index_count, 1, 0, 0, 0);
  }
}

} // namespace demo