// SPDX-License-Identifier: MIT
#include <demo/terrain/terrain_module.hpp>

namespace demo {

terrain_module::terrain_module()
: _grid{config.world_width, config.world_height},
  _heightmap{config.world_width, config.world_height, config.generation},
  _splat_weights{generate_splat_map(_heightmap, config.splat)},
  _splat_dirty{true} {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  _height_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
  _splat_buffer = graphics_module.add_resource<sbx::graphics::storage_buffer>(sbx::graphics::storage_buffer::min_size, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

  sbx::utility::logger<"terrain">::info("Initialized {}x{} terrain (cell size: {}m)", config.world_width, config.world_height, grid::cell_size);
}

auto terrain_module::update() -> void {
  _upload_gpu_data();
}

auto terrain_module::height_buffer() const -> sbx::graphics::storage_buffer_handle {
  return _height_buffer;
}

auto terrain_module::splat_buffer() const -> sbx::graphics::storage_buffer_handle {
  return _splat_buffer;
}

auto terrain_module::grid() -> demo::grid& {
  return _grid;
}

auto terrain_module::grid() const -> const demo::grid& {
  return _grid;
}

auto terrain_module::heightmap() -> demo::heightmap& {
  return _heightmap;
}

auto terrain_module::heightmap() const -> const demo::heightmap& {
  return _heightmap;
}

auto terrain_module::splat_data() const -> const splat_weights* {
  return _splat_weights.data();
}

auto terrain_module::splat_data_size_bytes() const -> std::size_t {
  return _splat_weights.size() * sizeof(splat_weights);
}

auto terrain_module::offset_x() const -> std::float_t {
  return _grid.offset_x();
}

auto terrain_module::offset_z() const -> std::float_t {
  return _grid.offset_z();
}

auto terrain_module::cell_size() const -> std::float_t {
  return grid::cell_size;
}

auto terrain_module::world_to_cell(std::float_t world_x, std::float_t world_z) const -> chunk_coord {
  auto cell_x = static_cast<std::int32_t>((world_x + offset_x()) / cell_size());
  auto cell_z = static_cast<std::int32_t>((world_z + offset_z()) / cell_size());

  return chunk_coord{cell_x, cell_z};
}

auto terrain_module::cell_to_world(std::int32_t cell_x, std::int32_t cell_z) const -> std::pair<std::float_t, std::float_t> {
  auto world_x = static_cast<std::float_t>(cell_x) * cell_size() - offset_x();
  auto world_z = static_cast<std::float_t>(cell_z) * cell_size() - offset_z();

  return {world_x, world_z};
}

auto terrain_module::get_height_at(std::float_t world_x, std::float_t world_z) const -> std::float_t {
  return _heightmap.get_height_at(world_x, world_z);
}

auto terrain_module::get_slope_at(std::int32_t cell_x, std::int32_t cell_z) const -> std::float_t {
  return get_slope_cost(_heightmap, cell_x, cell_z);
}

auto terrain_module::sculpt(std::float_t world_x, std::float_t world_z, std::float_t radius, std::float_t strength) -> sculpt_result {
  auto result = sculpt_terrain(_heightmap, world_x, world_z, radius, strength);

  regenerate_splat_region(_splat_weights, _heightmap, result.min_vertex_x, result.min_vertex_z, result.max_vertex_x, result.max_vertex_z, config.splat);
  _splat_dirty = true;

  return result;
}

auto terrain_module::flatten(std::int32_t cell_x, std::int32_t cell_z, std::uint32_t size_width, std::uint32_t size_height) -> void {
  flatten_for_building(_heightmap, cell_x, cell_z, size_width, size_height);
}

auto terrain_module::get_height_at_cell(std::int32_t cell_x, std::int32_t cell_z) const -> std::float_t {
  auto [world_x, world_z] = cell_to_world(cell_x, cell_z);

  return _heightmap.get_height_at(world_x, world_z);
}

auto terrain_module::can_build_at(std::int32_t cell_x, std::int32_t cell_z, std::uint32_t size_width, std::uint32_t size_height, std::float_t max_slope) const -> bool {
  for (auto z = cell_z; z < cell_z + static_cast<std::int32_t>(size_height); ++z) {
    for (auto x = cell_x; x < cell_x + static_cast<std::int32_t>(size_width); ++x) {
      if (!_grid.in_bounds(x, z)) {
        return false;
      }

      const auto& grid_cell = _grid.at(x, z);

      if (grid_cell.building_id != 0) {
        return false;
      }

      if (grid_cell.terrain_type == 3) {
        return false;
      }

      if (get_slope_cost(_heightmap, x, z) > max_slope) {
        return false;
      }
    }
  }

  return true;
}

auto terrain_module::world_width() const -> std::uint32_t {
  return config.world_width;
}

auto terrain_module::world_height() const -> std::uint32_t {
  return config.world_height;
}

auto terrain_module::_upload_gpu_data() -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  if (_heightmap.is_dirty()) {
    auto& storage = graphics_module.get_resource<sbx::graphics::storage_buffer>(_height_buffer);
    auto required_size = static_cast<VkDeviceSize>(_heightmap.data_size_bytes());

    if (required_size > storage.size()) {
      storage.resize(required_size);
    }

    storage.update(_heightmap.data(), _heightmap.data_size_bytes());
    _heightmap.clear_dirty();
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
}

} // namespace demo