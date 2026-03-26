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

  auto albedo_paths = std::array<std::filesystem::path, road_texture_count>{
    "res://textures/road/dirt/albedo.jpg",
    "res://textures/road/gravel/albedo.jpg",
    "res://textures/road/paved/albedo.jpg",
    "res://textures/road/highway/albedo.jpg",
  };

  auto normal_paths = std::array<std::filesystem::path, road_texture_count>{
    "res://textures/road/dirt/normal.jpg",
    "res://textures/road/gravel/normal.jpg",
    "res://textures/road/paved/normal.jpg",
    "res://textures/road/highway/normal.jpg",
  };

  for (auto i = 0u; i < road_texture_count; ++i) {
    _road_texture_handles[i] = graphics_module.add_resource<sbx::graphics::image2d>(
      albedo_paths[i],
      sbx::graphics::format::r8g8b8a8_srgb,
      VK_FILTER_LINEAR,
      VK_SAMPLER_ADDRESS_MODE_REPEAT,
      true,
      true
    );

    _road_images.push_back(_road_texture_handles[i]);
  }

  for (auto i = 0u; i < road_texture_count; ++i) {
    _road_texture_handles[road_texture_count + i] = graphics_module.add_resource<sbx::graphics::image2d>(
      normal_paths[i],
      sbx::graphics::format::r8g8b8a8_unorm,
      VK_FILTER_LINEAR,
      VK_SAMPLER_ADDRESS_MODE_REPEAT,
      true,
      true
    );

    _road_images.push_back(_road_texture_handles[road_texture_count + i]);
  }

  _road_sampler = graphics_module.add_resource<sbx::graphics::sampler_state>(
    sbx::graphics::filter::linear,
    sbx::graphics::filter::linear,
    sbx::graphics::address_mode::repeat,
    sbx::graphics::address_mode::repeat,
    8.0f,
    VK_LOD_CLAMP_NONE
  );
}

auto road_subrenderer::mark_dirty() -> void {
  _is_dirty = true;
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

  if (building_module.roads_dirty() || _is_dirty) {
    auto mesh = build_road_mesh(terrain_module.grid(), terrain_module.heightmap());

    if (mesh.is_empty) {
      _index_count = 0;
    } else {
      auto& vb = graphics_module.get_resource<sbx::graphics::storage_buffer>(_vertex_buffer);
      _update_buffer(vb, mesh.vertices);

      auto& ib = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);
      _update_buffer(ib, mesh.indices);

      _index_count = static_cast<std::uint32_t>(mesh.indices.size());
    }

    _is_dirty = false;
    building_module.clear_roads_dirty();
  }

  if (_index_count == 0) {
    return;
  }

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", environment.uniform_handler());
  _descriptor_handler.push("road_textures", _road_images);
  _descriptor_handler.push("road_sampler", graphics_module.get_resource<sbx::graphics::sampler_state>(_road_sampler));

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
