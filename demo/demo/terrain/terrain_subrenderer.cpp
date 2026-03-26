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

  auto indices = _generate_grid_indices();
  _index_count = static_cast<std::uint32_t>(indices.size());

  _index_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  auto& index_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);
  auto required_index_size = static_cast<VkDeviceSize>(indices.size() * sizeof(std::uint32_t));

  if (required_index_size > index_storage.size()) {
    index_storage.resize(required_index_size);
  }

  index_storage.update(indices.data(), indices.size() * sizeof(std::uint32_t));

  auto layer_paths = std::array<std::filesystem::path, terrain_layer_count>{
    "res://textures/terrain/grass/albedo.jpg",
    "res://textures/terrain/dirt/albedo.jpg",
    "res://textures/terrain/rock/albedo.jpg",
    "res://textures/terrain/sand/albedo.jpg",
    "res://textures/terrain/snow/albedo.jpg",
    "res://textures/terrain/mud/albedo.jpg",
  };

  for (auto i = 0u; i < terrain_layer_count; ++i) {
    _layer_textures[i] = graphics_module.add_resource<sbx::graphics::image2d>(layer_paths[i], sbx::graphics::format::r8g8b8a8_srgb, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, true, true);
    _terrain_images.push_back(_layer_textures[i]);
  }

  _terrain_sampler = graphics_module.add_resource<sbx::graphics::sampler_state>(sbx::graphics::filter::linear, sbx::graphics::filter::linear, sbx::graphics::address_mode::repeat, sbx::graphics::address_mode::repeat, 8.0f, VK_LOD_CLAMP_NONE);

  auto macro_noise = std::vector<std::uint8_t>{};
  auto macro_dimension = 64u;
  macro_noise.resize(macro_dimension * macro_dimension);

  for (auto i = 0u; i < macro_noise.size(); ++i) {
    macro_noise[i] = static_cast<std::uint8_t>(sbx::math::random::next<std::float_t>(0.0f, 1.0f) * 255.0f);
  }

  _noise_image = graphics_module.add_resource<sbx::graphics::image2d>(
    sbx::math::vector2u{macro_dimension, macro_dimension},
    sbx::graphics::format::r8_unorm,
    sbx::graphics::filter::linear,
    macro_noise.data()
  );
}

auto terrain_subrenderer::_generate_grid_indices() -> std::vector<std::uint32_t> {
  auto indices = std::vector<std::uint32_t>{};
  indices.reserve(clipmap_grid_size * clipmap_grid_size * 6);

  for (auto row = 0u; row < clipmap_grid_size; ++row) {
    for (auto col = 0u; col < clipmap_grid_size; ++col) {
      auto top_left = row * clipmap_grid_verts + col;
      auto top_right = top_left + 1;
      auto bottom_left = (row + 1) * clipmap_grid_verts + col;
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

auto terrain_subrenderer::render(sbx::graphics::command_buffer& command_buffer) -> void {
  EASY_BLOCK("terrain_subrenderer::render", profiler::colors::Green);
  SBX_PROFILE_SCOPE("terrain_subrenderer::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  auto camera_node = environment.camera();
  auto camera_position = graph.world_position(camera_node);

  auto& noise_image = graphics_module.get_resource<sbx::graphics::image2d>(_noise_image);

  _pipeline.bind(command_buffer);

  _descriptor_handler.push("scene", environment.uniform_handler());
  _descriptor_handler.push("terrain_textures", _terrain_images);
  _descriptor_handler.push("terrain_sampler", graphics_module.get_resource<sbx::graphics::sampler_state>(_terrain_sampler));
  _descriptor_handler.push("noise_image", noise_image);

  if (!_descriptor_handler.update(_pipeline)) {
    return;
  }

  _descriptor_handler.bind_descriptors(command_buffer);

  auto& index_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_index_buffer);
  command_buffer.bind_index_buffer(index_storage.handle(), 0, VK_INDEX_TYPE_UINT32);

  auto& height_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(terrain_module.height_buffer());
  auto& splat_storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(terrain_module.splat_buffer());

  auto& height_map = terrain_module.heightmap();
  auto base_cell_size = terrain_module.cell_size();
  auto terrain_offset_x = terrain_module.offset_x();
  auto terrain_offset_z = terrain_module.offset_z();
  auto map_width = static_cast<std::float_t>(terrain_module.world_width()) * base_cell_size;
  auto map_height = static_cast<std::float_t>(terrain_module.world_height()) * base_cell_size;

  for (auto ring = 0u; ring < clipmap_ring_count; ++ring) {
    auto ring_cell_size = base_cell_size * static_cast<std::float_t>(1u << ring);

    auto ring_origin_x = std::floor(camera_position.x() / ring_cell_size) * ring_cell_size;
    auto ring_origin_z = std::floor(camera_position.z() / ring_cell_size) * ring_cell_size;

    auto inner_min_x = 0.0f;
    auto inner_min_z = 0.0f;
    auto inner_max_x = 0.0f;
    auto inner_max_z = 0.0f;

    if (ring > 0) {
      auto inner_cell_size = base_cell_size * static_cast<std::float_t>(1u << (ring - 1));
      auto inner_origin_x = std::floor(camera_position.x() / inner_cell_size) * inner_cell_size;
      auto inner_origin_z = std::floor(camera_position.z() / inner_cell_size) * inner_cell_size;
      auto inner_half = static_cast<std::float_t>(clipmap_grid_size) * inner_cell_size * 0.5f;

      inner_min_x = inner_origin_x - inner_half;
      inner_min_z = inner_origin_z - inner_half;
      inner_max_x = inner_origin_x + inner_half;
      inner_max_z = inner_origin_z + inner_half;
    }

    _push_handler.push("height_data_buffer", height_storage.address());
    _push_handler.push("splat_data_buffer", splat_storage.address());
    _push_handler.push("grid_verts", clipmap_grid_verts);
    _push_handler.push("grid_cells", clipmap_grid_size);
    _push_handler.push("ring_cell_size", ring_cell_size);
    _push_handler.push("ring_origin_x", ring_origin_x);
    _push_handler.push("ring_origin_z", ring_origin_z);
    _push_handler.push("ring_level", ring);
    _push_handler.push("base_cell_size", base_cell_size);
    _push_handler.push("terrain_offset_x", terrain_offset_x);
    _push_handler.push("terrain_offset_z", terrain_offset_z);
    _push_handler.push("terrain_width", map_width);
    _push_handler.push("terrain_height", map_height);
    _push_handler.push("vertices_x", height_map.vertices_x());
    _push_handler.push("vertices_z", height_map.vertices_z());
    _push_handler.push("cells_x", terrain_module.world_width());
    _push_handler.push("cells_z", terrain_module.world_height());
    _push_handler.push("inner_min_x", inner_min_x);
    _push_handler.push("inner_min_z", inner_min_z);
    _push_handler.push("inner_max_x", inner_max_x);
    _push_handler.push("inner_max_z", inner_max_z);
    _push_handler.push("ring_count", clipmap_ring_count);
    _push_handler.push("tiling_scale", _tiling_scale);

    _push_handler.bind(command_buffer);

    command_buffer.draw_indexed(_index_count, 1, 0, 0, 0);
  }
}

} // namespace demo
