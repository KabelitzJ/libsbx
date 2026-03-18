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
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  auto& height_map = terrain_module.heightmap();

  _height_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

  auto& height_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_height_buffer);
  auto required_size = static_cast<VkDeviceSize>(height_map.data_size_bytes());

  if (required_size > height_storage.size()) {
    height_storage.resize(required_size);
  }

  height_storage.update(height_map.data(), height_map.data_size_bytes());
}

auto water_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("water_subrenderer::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();

  // Always re-upload: terrain_subrenderer clears heightmap dirty before we run
  auto& height_map = terrain_module.heightmap();

  {
    auto& height_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_height_buffer);
    auto required_size = static_cast<VkDeviceSize>(height_map.data_size_bytes());

    if (required_size > height_storage.size()) {
      height_storage.resize(required_size);
    }

    height_storage.update(height_map.data(), height_map.data_size_bytes());
  }

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", environment.uniform_handler());

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  auto& height_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_height_buffer);

  auto terrain_width = static_cast<std::float_t>(terrain_module.world_width()) * terrain_module.cell_size();
  auto terrain_height = static_cast<std::float_t>(terrain_module.world_height()) * terrain_module.cell_size();

  _push_handler.push("height_data_buffer", height_storage.address());
  _push_handler.push("chunk_world_x", -terrain_module.offset_x());
  _push_handler.push("chunk_world_z", -terrain_module.offset_z());
  _push_handler.push("chunk_size", std::max(terrain_width, terrain_height));
  _push_handler.push("water_level", terrain_constants::sea_level);
  _push_handler.push("terrain_offset_x", terrain_module.offset_x());
  _push_handler.push("terrain_offset_z", terrain_module.offset_z());
  _push_handler.push("base_cell_size", terrain_module.cell_size());
  _push_handler.push("vertices_x", height_map.vertices_x());
  _push_handler.push("vertices_z", height_map.vertices_z());

  _push_handler.bind(command_buffer);

  command_buffer.draw(6, 1, 0, 0);
}

} // namespace demo
