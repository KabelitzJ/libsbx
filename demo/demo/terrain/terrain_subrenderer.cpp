// SPDX-License-Identifier: MIT
#include <demo/terrain/terrain_subrenderer.hpp>

#include <libsbx/scenes/scenes.hpp>
#include <libsbx/graphics/graphics.hpp>

namespace demo {

terrain_subrenderer::terrain_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path)
: sbx::graphics::subrenderer{},
  _pipeline{path, attachments},
  _push_handler{_pipeline},
  _descriptor_handler{_pipeline, 0u} {

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto indices = _generate_chunk_indices();
  _index_count = static_cast<std::uint32_t>(indices.size());

  _index_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  auto& index_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);

  _update_buffer(index_buffer, indices);
}

auto terrain_subrenderer::_generate_chunk_indices() -> std::vector<std::uint32_t> {
  auto indices = std::vector<std::uint32_t>{};
  indices.reserve(chunk_size * chunk_size * 6);

  for (auto y = 0u; y < chunk_size; ++y) {
    for (auto x = 0u; x < chunk_size; ++x) {
      auto top_left = y * chunk_vertices + x;
      auto top_right = top_left + 1;
      auto bottom_left = (y + 1) * chunk_vertices + x;
      auto bottom_right = bottom_left + 1;

      indices.push_back(top_left);
      indices.push_back(bottom_left);
      indices.push_back(top_right);

      indices.push_back(top_right);
      indices.push_back(bottom_left);
      indices.push_back(bottom_right);
    }
  }

  return indices;
}

auto terrain_subrenderer::_ensure_gpu_chunk(chunk_coord chunk_coordinates) -> gpu_chunk_data& {
  auto it = _gpu_chunks.find(chunk_coordinates);

  if (it != _gpu_chunks.end()) {
    return it->second;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto& data = _gpu_chunks[chunk_coordinates];

  data.height_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  data.splat_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  data.height_uploaded = false;
  data.splat_uploaded = false;

  return data;
}

auto terrain_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("terrain_subrenderer::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();
  auto& asset_registry = scenes_module.asset_registry();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto camera_node = environment.camera();
  auto& camera_transform = graph.get_component<sbx::scenes::transform>(camera_node);
  auto camera_position = camera_transform.position();

  auto visible_chunks = terrain_module.get_visible_chunks(camera_position.x(), camera_position.z(), _view_distance);

  if (visible_chunks.empty()) {
    return;
  }

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", environment.uniform_handler());

  // _descriptor_handler.push("grass_image", graphics_module.get_resource<sbx::graphics::image2d>(asset_registry.get_image("terrain_grass")));
  // _descriptor_handler.push("dirt_image", graphics_module.get_resource<sbx::graphics::image2d>(asset_registry.get_image("terrain_dirt")));
  // _descriptor_handler.push("rock_image", graphics_module.get_resource<sbx::graphics::image2d>(asset_registry.get_image("terrain_rock")));
  // _descriptor_handler.push("sand_image", graphics_module.get_resource<sbx::graphics::image2d>(asset_registry.get_image("terrain_sand")));
  // _descriptor_handler.push("snow_image", graphics_module.get_resource<sbx::graphics::image2d>(asset_registry.get_image("terrain_snow")));
  // _descriptor_handler.push("mud_image", graphics_module.get_resource<sbx::graphics::image2d>(asset_registry.get_image("terrain_mud")));

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  auto& index_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);
  command_buffer.bind_index_buffer(index_buffer.handle(), 0, VK_INDEX_TYPE_UINT32);

  auto terrain_cell_size = terrain_module.cell_size();
  auto terrain_offset_x = terrain_module.offset_x();
  auto terrain_offset_z = terrain_module.offset_z();
  auto inverse_chunk_vertices = 1.0f / static_cast<std::float_t>(chunk_vertices);

  for (const auto& chunk_coordinates : visible_chunks) {
    terrain_module.heightmap().ensure_chunk(chunk_coordinates);

    auto* height_chunk_data = terrain_module.heightmap().get_chunk(chunk_coordinates);

    if (!height_chunk_data) {
      continue;
    }

    auto& splat_chunk_data = terrain_module.ensure_splat_chunk(chunk_coordinates);
    auto& gpu_chunk = _ensure_gpu_chunk(chunk_coordinates);

    // Upload heights if dirty
    if (!gpu_chunk.height_uploaded || height_chunk_data->is_dirty) {
      auto& height_storage_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(gpu_chunk.height_buffer);
      _update_buffer(height_storage_buffer, height_chunk_data->heights);
      height_chunk_data->is_dirty = false;
      gpu_chunk.height_uploaded = true;
    }

    // Upload splat if dirty
    if (!gpu_chunk.splat_uploaded || splat_chunk_data.is_dirty) {
      auto& splat_storage_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(gpu_chunk.splat_buffer);
      _update_buffer(splat_storage_buffer, splat_chunk_data.weights);
      splat_chunk_data.is_dirty = false;
      gpu_chunk.splat_uploaded = true;
    }

    auto& height_storage_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(gpu_chunk.height_buffer);
    auto& splat_storage_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(gpu_chunk.splat_buffer);

    auto chunk_world_x = static_cast<std::float_t>(chunk_coordinates.x * static_cast<std::int32_t>(chunk_size)) * terrain_cell_size - terrain_offset_x;
    auto chunk_world_z = static_cast<std::float_t>(chunk_coordinates.y * static_cast<std::int32_t>(chunk_size)) * terrain_cell_size - terrain_offset_z;

    _push_handler.push("height_data_buffer", height_storage_buffer.address());
    _push_handler.push("splat_data_buffer", splat_storage_buffer.address());
    _push_handler.push("chunk_verts", chunk_vertices);
    _push_handler.push("chunk_cells", chunk_size);
    _push_handler.push("cell_size", terrain_cell_size);
    _push_handler.push("chunk_world_x", chunk_world_x);
    _push_handler.push("chunk_world_z", chunk_world_z);
    _push_handler.push("inv_chunk_verts", inverse_chunk_vertices);
    _push_handler.push("tiling_scale", _tiling_scale);

    _push_handler.bind(command_buffer);

    command_buffer.draw_indexed(_index_count, 1, 0, 0, 0);
  }
}

} // namespace demo
