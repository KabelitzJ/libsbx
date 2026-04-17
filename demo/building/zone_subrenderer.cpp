// SPDX-License-Identifier: MIT
#include <demo/building/zone_subrenderer.hpp>

#include <libsbx/scenes/scenes.hpp>
#include <libsbx/graphics/graphics.hpp>
#include <libsbx/utility/logger.hpp>

#include <demo/building/building_module.hpp>

namespace demo {

zone_subrenderer::zone_subrenderer(const std::vector<sbx::graphics::attachment_description>& attachments, const std::filesystem::path& path)
: sbx::graphics::subrenderer{},
  _pipeline{path, attachments},
  _push_handler{_pipeline},
  _descriptor_handler{_pipeline, 0u} {

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  _vertex_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(
    sbx::graphics::storage_buffer::min_size,
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  );

  _index_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(
    sbx::graphics::storage_buffer::min_size,
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
  );
}

auto zone_subrenderer::mark_dirty() -> void {
  _is_dirty = true;
}

auto zone_subrenderer::set_visible(bool visible) -> void {
  _is_visible = visible;
}

auto zone_subrenderer::is_visible() const -> bool {
  return _is_visible;
}

auto zone_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  EASY_BLOCK("zone_subrenderer::render", profiler::colors::Cyan);
  SBX_PROFILE_SCOPE("zone_subrenderer::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();
  auto& building_module = sbx::core::engine::get_module<demo::building_module>();

  if (!_is_visible && !building_module.is_zone_mode()) {
    return;
  }

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();

  if (_is_dirty || building_module.zones_dirty()) {
    auto cell_to_world = [&](cell_coordinates coords) -> world_coordinates {
      return terrain_module.cell_to_world(coords);
    };

    auto mesh = build_zone_overlay_mesh(terrain_module.grid(), terrain_module.heightmap(), cell_to_world);

    if (mesh.is_empty) {
      _index_count = 0;
    } else {
      auto& vb = graphics_module.get_resource<sbx::graphics::storage_buffer>(_vertex_buffer);
      _update_buffer(vb, mesh.vertices);

      auto& ib = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);
      _update_buffer(ib, mesh.indices);

      _index_count = static_cast<std::uint32_t>(mesh.indices.size());

      sbx::utility::logger<"zone">::debug("Zone mesh rebuilt: {} vertices, {} indices", mesh.vertices.size(), mesh.indices.size());
    }

    _is_dirty = false;
    building_module.clear_zones_dirty();
  }

  if (_index_count == 0) {
    return;
  }

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", environment.uniform_handler());

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  auto& vb = graphics_module.get_resource<sbx::graphics::storage_buffer>(_vertex_buffer);
  auto& ib = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);

  _push_handler.push("vertex_data_buffer", vb.address());
  _push_handler.bind(command_buffer);

  command_buffer.bind_index_buffer(ib.handle(), 0, VK_INDEX_TYPE_UINT32);
  command_buffer.draw_indexed(_index_count, 1, 0, 0, 0);
}

} // namespace demo
