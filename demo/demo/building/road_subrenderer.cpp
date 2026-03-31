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

  _preview_vertex_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(
    sbx::graphics::storage_buffer::min_size,
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  );

  _preview_index_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(
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
    auto mesh = build_road_mesh();

    if (mesh.is_empty) {
      _index_count = 0;
    } else {
      auto& vertex_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_vertex_buffer);
      _update_buffer(vertex_buffer, mesh.vertices);

      auto& index_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);
      _update_buffer(index_buffer, mesh.indices);

      _index_count = static_cast<std::uint32_t>(mesh.indices.size());
    }

    _is_dirty = false;
    building_module.clear_roads_dirty();
  }

  if (building_module.road_preview_dirty()) {
    auto& preview = building_module.get_road_preview();

    if (preview.active && !preview.preview_cells.empty()) {
      auto preview_mesh = build_road_preview_mesh(preview.preview_cells, preview.type);

      if (preview_mesh.is_empty) {
        _preview_index_count = 0;
      } else {
        auto& pvb = graphics_module.get_resource<sbx::graphics::storage_buffer>(_preview_vertex_buffer);
        _update_buffer(pvb, preview_mesh.vertices);

        auto& pib = graphics_module.get_resource<sbx::graphics::storage_buffer>(_preview_index_buffer);
        _update_buffer(pib, preview_mesh.indices);

        _preview_index_count = static_cast<std::uint32_t>(preview_mesh.indices.size());
      }
    } else {
      _preview_index_count = 0;
    }

    building_module.clear_road_preview_dirty();
  }

  if (_index_count == 0 && _preview_index_count == 0) {
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

  if (_index_count > 0) {
    auto& vertex_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_vertex_buffer);
    auto& index_buffer = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);

    _push_handler.push("vertex_data_buffer", vertex_buffer.address());
    _push_handler.push("preview_mode", static_cast<std::uint32_t>(0));
    _push_handler.bind(command_buffer);

    command_buffer.bind_index_buffer(index_buffer.handle(), 0, VK_INDEX_TYPE_UINT32);
    command_buffer.draw_indexed(_index_count, 1, 0, 0, 0);
  }

  if (_preview_index_count > 0) {
    auto& pvb = graphics_module.get_resource<sbx::graphics::storage_buffer>(_preview_vertex_buffer);
    auto& pib = graphics_module.get_resource<sbx::graphics::storage_buffer>(_preview_index_buffer);

    _push_handler.push("vertex_data_buffer", pvb.address());
    _push_handler.push("preview_mode", static_cast<std::uint32_t>(1));
    _push_handler.bind(command_buffer);

    command_buffer.bind_index_buffer(pib.handle(), 0, VK_INDEX_TYPE_UINT32);
    command_buffer.draw_indexed(_preview_index_count, 1, 0, 0, 0);
  }
}

} // namespace demo
