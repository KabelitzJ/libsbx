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

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", environment.uniform_handler());

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  auto terrain_width = static_cast<std::float_t>(terrain_module.world_width()) * terrain_module.cell_size();
  auto terrain_height = static_cast<std::float_t>(terrain_module.world_height()) * terrain_module.cell_size();

  _push_handler.push("chunk_world_x", -terrain_module.offset_x());
  _push_handler.push("chunk_world_z", -terrain_module.offset_z());
  _push_handler.push("chunk_size", std::max(terrain_width, terrain_height));
  _push_handler.push("water_level", terrain_constants::sea_level);

  _push_handler.bind(command_buffer);

  command_buffer.draw(6, 1, 0, 0);
}

} // namespace demo
