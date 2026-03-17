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
  _descriptor_handler{_pipeline, 0u} { }

auto water_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("water_subrenderer::render");

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

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

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  auto terrain_cell_size = terrain_module.cell_size();
  auto terrain_offset_x = terrain_module.offset_x();
  auto terrain_offset_z = terrain_module.offset_z();

  for (const auto& chunk_coordinates : visible_chunks) {
    auto chunk_world_x = static_cast<std::float_t>(chunk_coordinates.x * static_cast<std::int32_t>(chunk_size)) * terrain_cell_size - terrain_offset_x;
    auto chunk_world_z = static_cast<std::float_t>(chunk_coordinates.y * static_cast<std::int32_t>(chunk_size)) * terrain_cell_size - terrain_offset_z;
    auto chunk_world_size = static_cast<std::float_t>(chunk_size) * terrain_cell_size;

    _push_handler.push("chunk_world_x", chunk_world_x);
    _push_handler.push("chunk_world_z", chunk_world_z);
    _push_handler.push("chunk_size", chunk_world_size);
    _push_handler.push("water_level", terrain_constants::sea_level);

    _push_handler.bind(command_buffer);

    // 6 vertices = 2 triangles = 1 quad
    command_buffer.draw(6, 1, 0, 0);
  }
}

} // namespace demo
