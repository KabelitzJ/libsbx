// SPDX-License-Identifier: MIT
#include <demo/terrain/border_subrenderer.hpp>

#include <libsbx/graphics/graphics.hpp>
#include <libsbx/scenes/scenes.hpp>

#include <demo/terrain/terrain_module.hpp>

namespace demo {

border_subrenderer::border_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path)
: sbx::graphics::subrenderer{},
  _pipeline{path, attachments},
  _push_handler{_pipeline},
  _descriptor_handler{_pipeline, 0u} { }

auto border_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  SBX_PROFILE_SCOPE("border_subrenderer::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  if (!terrain_module.is_loaded() || terrain_module.edge_count() == 0u) {
    return;
  }

  auto& scene = scenes_module.active_scene();
  auto& environment = scene.environment();

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", environment.uniform_handler());

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  auto& height_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(terrain_module.height_buffer());
  auto& borders_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(terrain_module.borders_buffer());

  _push_handler.push("height_data_buffer", height_storage.address());
  _push_handler.push("edges_buffer", borders_storage.address());
  _push_handler.push("vertices_x", terrain_module.vertices_x());
  _push_handler.push("vertices_z", terrain_module.vertices_z());
  _push_handler.push("base_cell_size", terrain_module.cell_size());
  _push_handler.push("terrain_offset_x", terrain_module.offset_x());
  _push_handler.push("terrain_offset_z", terrain_module.offset_z());
  _push_handler.push("width", _width);
  _push_handler.push("height_offset", _height_offset);
  _push_handler.push("subdivisions", _subdivisions);
  _push_handler.push("color", _color);

  _push_handler.bind(command_buffer);

  command_buffer.draw(6u * _subdivisions, terrain_module.edge_count(), 0u, 0u);
}

} // namespace demo
