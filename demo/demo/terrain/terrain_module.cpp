// SPDX-License-Identifier: MIT
#include <demo/terrain/terrain_module.hpp>

#include <fstream>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/utility/logger.hpp>

namespace demo {

terrain_module::terrain_module()
: _splat_dirty{false},
  _province_dirty{false},
  _borders_dirty{false},
  _tints_dirty{false},
  _loaded{false} {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  _height_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _splat_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _province_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _borders_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _tints_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
}

auto terrain_module::update() -> void {
  if (!_loaded) {
    return;
  }

  _upload_gpu_data();
}

auto terrain_module::load() -> void {
  if (_loaded) {
    return;
  }

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  auto heightmap_path = assets_module.resolve_path(config.heightmap_path);
  auto province_ids_path = assets_module.resolve_path(config.province_ids_path);
  auto borders_path = assets_module.resolve_path(config.borders_path);
  auto tints_path = assets_module.resolve_path(config.tints_path);
  auto metadata_path = assets_module.resolve_path(config.metadata_path);

  auto info = _read_metadata(metadata_path);

  _heightmap.emplace(heightmap_path, info.width, info.height, config.cell_size, config.height_scale);
  _province_map.emplace(province_ids_path, metadata_path, info.width, info.height, config.cell_size);
  _splat_weights = generate_splat_from_provinces(*_province_map, *_heightmap, config.splat);
  _splat_dirty = true;
  _province_dirty = true;

  // Load Voronoi edges: 4 floats per edge (start_x, start_y, end_x, end_y),
  // in centered pixel coordinates. Convert to world XZ via cell_size.
  _edge_count = info.edge_count;
  _edges_world.resize(static_cast<std::size_t>(_edge_count) * 4u);

  auto edges_size = _edges_world.size() * sizeof(std::float_t);
  auto edges_stream = std::ifstream{borders_path, std::ios::binary};

  if (!edges_stream) {
    throw std::runtime_error{"failed to open: " + borders_path.string()};
  }

  edges_stream.read(reinterpret_cast<char*>(_edges_world.data()), static_cast<std::streamsize>(edges_size));

  if (static_cast<std::size_t>(edges_stream.gcount()) != edges_size) {
    throw std::runtime_error{"short read on: " + borders_path.string()};
  }

  for (auto i = std::size_t{0}; i < _edges_world.size(); ++i) {
    _edges_world[i] *= config.cell_size;
  }

  _borders_dirty = true;

  // Per-province biome tints: 4 floats (rgba) per province.
  auto n_provinces = _province_map->province_count();
  _tints.resize(static_cast<std::size_t>(n_provinces) * 4u);

  auto tints_size = _tints.size() * sizeof(std::float_t);
  auto tints_stream = std::ifstream{tints_path, std::ios::binary};

  if (!tints_stream) {
    throw std::runtime_error{"failed to open: " + tints_path.string()};
  }

  tints_stream.read(reinterpret_cast<char*>(_tints.data()), static_cast<std::streamsize>(tints_size));

  if (static_cast<std::size_t>(tints_stream.gcount()) != tints_size) {
    throw std::runtime_error{"short read on: " + tints_path.string()};
  }

  _tints_dirty = true;
  _loaded = true;

  sbx::utility::logger<"terrain">::info("Loaded terrain {}x{} (cell size: {}m, height scale: {}m, provinces: {}, edges: {})", info.width, info.height, config.cell_size, config.height_scale, _province_map->province_count(), _edge_count);
}

auto terrain_module::is_loaded() const -> bool {
  return _loaded;
}

auto terrain_module::height_buffer() const -> sbx::graphics::storage_buffer_handle {
  return _height_buffer;
}

auto terrain_module::splat_buffer() const -> sbx::graphics::storage_buffer_handle {
  return _splat_buffer;
}

auto terrain_module::province_buffer() const -> sbx::graphics::storage_buffer_handle {
  return _province_buffer;
}

auto terrain_module::borders_buffer() const -> sbx::graphics::storage_buffer_handle {
  return _borders_buffer;
}

auto terrain_module::tints_buffer() const -> sbx::graphics::storage_buffer_handle {
  return _tints_buffer;
}

auto terrain_module::edge_count() const -> std::uint32_t {
  return _edge_count;
}

auto terrain_module::heightmap() -> demo::heightmap& {
  return *_heightmap;
}

auto terrain_module::heightmap() const -> const demo::heightmap& {
  return *_heightmap;
}

auto terrain_module::provinces() -> demo::province_map& {
  return *_province_map;
}

auto terrain_module::provinces() const -> const demo::province_map& {
  return *_province_map;
}

auto terrain_module::cell_size() const -> std::float_t {
  return _heightmap->cell_size();
}

auto terrain_module::height_scale() const -> std::float_t {
  return _heightmap->height_scale();
}

auto terrain_module::vertices_x() const -> std::uint32_t {
  return _heightmap->vertices_x();
}

auto terrain_module::vertices_z() const -> std::uint32_t {
  return _heightmap->vertices_z();
}

auto terrain_module::cells_x() const -> std::uint32_t {
  return _heightmap->vertices_x() - 1u;
}

auto terrain_module::cells_z() const -> std::uint32_t {
  return _heightmap->vertices_z() - 1u;
}

auto terrain_module::world_extent_x() const -> std::float_t {
  return static_cast<std::float_t>(cells_x()) * cell_size();
}

auto terrain_module::world_extent_z() const -> std::float_t {
  return static_cast<std::float_t>(cells_z()) * cell_size();
}

auto terrain_module::offset_x() const -> std::float_t {
  return world_extent_x() * 0.5f;
}

auto terrain_module::offset_z() const -> std::float_t {
  return world_extent_z() * 0.5f;
}

auto terrain_module::get_height_at(const world_coordinates& coordinates) const -> std::float_t {
  return _heightmap->get_height_at(coordinates);
}

auto terrain_module::get_landform_at(const world_coordinates& coordinates) const -> landform {
  return _province_map->landform_at(coordinates);
}

auto terrain_module::get_province_at(const world_coordinates& coordinates) const -> province_map::province_id {
  return _province_map->province_at(coordinates);
}

auto terrain_module::splat_data() const -> const splat_weights* {
  return _splat_weights.data();
}

auto terrain_module::splat_data_size_bytes() const -> std::size_t {
  return _splat_weights.size() * sizeof(splat_weights);
}

auto terrain_module::_read_metadata(const std::filesystem::path& path) -> metadata_info {
  auto root = YAML::LoadFile(path.string());

  auto m = root["metadata"];

  if (!m) {
    throw std::runtime_error{"missing 'metadata' block in: " + path.string()};
  }

  return metadata_info{
    .width = m["width"].as<std::uint32_t>(),
    .height = m["height"].as<std::uint32_t>(),
    .edge_count = m["edge_count"] ? m["edge_count"].as<std::uint32_t>() : 0u,
  };
}

auto terrain_module::_upload_gpu_data() -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  if (_heightmap->is_dirty()) {
    auto& storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_height_buffer);
    auto required_size = static_cast<VkDeviceSize>(_heightmap->data_size_bytes());

    if (required_size > storage.size()) {
      storage.resize(required_size);
    }

    storage.update(_heightmap->data(), _heightmap->data_size_bytes());
    _heightmap->clear_dirty();
  }

  if (_splat_dirty) {
    auto& storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_splat_buffer);
    auto required_size = static_cast<VkDeviceSize>(splat_data_size_bytes());

    if (required_size > storage.size()) {
      storage.resize(required_size);
    }

    storage.update(_splat_weights.data(), splat_data_size_bytes());
    _splat_dirty = false;
  }

  if (_province_dirty && _province_map.has_value()) {
    auto& storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_province_buffer);
    auto required_size = static_cast<VkDeviceSize>(_province_map->ids_data_size_bytes());

    if (required_size > storage.size()) {
      storage.resize(required_size);
    }

    storage.update(_province_map->ids_data(), _province_map->ids_data_size_bytes());
    _province_dirty = false;
  }

  if (_borders_dirty) {
    auto& storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_borders_buffer);
    auto required_size = static_cast<VkDeviceSize>(_edges_world.size() * sizeof(std::float_t));

    if (required_size > storage.size()) {
      storage.resize(required_size);
    }

    storage.update(_edges_world.data(), _edges_world.size() * sizeof(std::float_t));
    _borders_dirty = false;
  }

  if (_tints_dirty) {
    auto& storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_tints_buffer);
    auto required_size = static_cast<VkDeviceSize>(_tints.size() * sizeof(std::float_t));

    if (required_size > storage.size()) {
      storage.resize(required_size);
    }

    storage.update(_tints.data(), _tints.size() * sizeof(std::float_t));
    _tints_dirty = false;
  }
}

} // namespace demo
