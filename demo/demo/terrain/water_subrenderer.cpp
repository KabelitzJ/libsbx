// SPDX-License-Identifier: MIT
#include <demo/terrain/water_subrenderer.hpp>

#include <libsbx/scenes/scenes.hpp>
#include <libsbx/graphics/graphics.hpp>

#include <demo/terrain/heightmap.hpp>

namespace demo {

water_subrenderer::water_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path)
: sbx::graphics::subrenderer{},
  _pipeline{path, attachments},
  _push_handler{_pipeline},
  _descriptor_handler{_pipeline, 0u} {

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  auto indices = _generate_grid_indices();
  _index_count = static_cast<std::uint32_t>(indices.size());

  _index_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  auto& index_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);
  auto required_size = static_cast<VkDeviceSize>(indices.size() * sizeof(std::uint32_t));

  if (required_size > index_storage.size()) {
    index_storage.resize(required_size);
  }

  index_storage.update(indices.data(), indices.size() * sizeof(std::uint32_t));
}

auto water_subrenderer::_generate_grid_indices() -> std::vector<std::uint32_t> {
  auto indices = std::vector<std::uint32_t>{};
  indices.reserve(water_grid_size * water_grid_size * 6);

  for (auto row = 0u; row < water_grid_size; ++row) {
    for (auto col = 0u; col < water_grid_size; ++col) {
      auto top_left = row * water_grid_vertices + col;
      auto top_right = top_left + 1;
      auto bottom_left = (row + 1) * water_grid_vertices + col;
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

auto water_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("water_subrenderer::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", environment.uniform_handler());

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  auto& index_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);
  command_buffer.bind_index_buffer(index_storage.handle(), 0, VK_INDEX_TYPE_UINT32);

  auto& height_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(terrain_module.height_buffer());

  auto& height_map = terrain_module.heightmap();
  auto terrain_width = static_cast<std::float_t>(terrain_module.world_width()) * terrain_module.cell_size();
  auto terrain_height = static_cast<std::float_t>(terrain_module.world_height()) * terrain_module.cell_size();
  auto plane_size = std::max(terrain_width, terrain_height);

  _push_handler.push("height_data_buffer", height_storage.address());
  _push_handler.push("origin_x", -terrain_module.offset_x());
  _push_handler.push("origin_z", -terrain_module.offset_z());
  _push_handler.push("plane_size", plane_size);
  _push_handler.push("water_level", terrain_constants::sea_level);
  _push_handler.push("grid_vertices", water_grid_vertices);
  _push_handler.push("grid_cells", water_grid_size);
  _push_handler.push("terrain_offset_x", terrain_module.offset_x());
  _push_handler.push("terrain_offset_z", terrain_module.offset_z());
  _push_handler.push("base_cell_size", terrain_module.cell_size());
  _push_handler.push("vertices_x", height_map.vertices_x());
  _push_handler.push("vertices_z", height_map.vertices_z());

  _push_handler.bind(command_buffer);

  command_buffer.draw_indexed(_index_count, 1, 0, 0, 0);
}

} // namespace demo
