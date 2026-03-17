// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_TERRAIN_MODULE_HPP_
#define DEMO_TERRAIN_TERRAIN_MODULE_HPP_

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cmath>
#include <utility>

#include <libsbx/core/module.hpp>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/grid.hpp>
#include <demo/terrain/heightmap.hpp>
#include <demo/terrain/sculpt.hpp>
#include <demo/terrain/splat_generator.hpp>

namespace demo {

struct terrain_config {
  std::uint32_t world_width{};
  std::uint32_t world_height{};
  terrain_generation_params generation{};
  splat_generation_params splat{};
  std::float_t water_percentile{0.15f};
}; // struct terrain_config

class terrain_module final : public sbx::core::module<terrain_module> {

  inline static const auto is_registered = register_module(stage::normal);

  inline static const auto config = terrain_config{
    .world_width = 1024,
    .world_height = 1024,
    .generation = {
      .base_scale = 0.003f,
      .height_scale = 30.0f,
      .river_width = 0.006f,
      .octaves = 3u,
      .seed_x = 42.0f,
      .seed_z = 73.0f,
    },
    .splat = {
      .variety_scale = 0.05f,
      .variety_strength = 0.15f,
      .moisture_scale = 0.02f,
      .seed = 42.0f,
    },
  };

public:

  terrain_module()
  : _grid{config.world_width, config.world_height},
    _heightmap{config.world_width, config.world_height, config.generation} {
    auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
    auto& asset_registry = scenes_module.asset_registry();

    // asset_registry.request_image("terrain_grass", "res://textures/terrain/grass.png", sbx::graphics::format::r8g8b8a8_srgb);
    // asset_registry.request_image("terrain_dirt", "res://textures/terrain/dirt.png", sbx::graphics::format::r8g8b8a8_srgb);
    // asset_registry.request_image("terrain_rock", "res://textures/terrain/rock.png", sbx::graphics::format::r8g8b8a8_srgb);
    // asset_registry.request_image("terrain_sand", "res://textures/terrain/sand.png", sbx::graphics::format::r8g8b8a8_srgb);
    // asset_registry.request_image("terrain_snow", "res://textures/terrain/snow.png", sbx::graphics::format::r8g8b8a8_srgb);
    // asset_registry.request_image("terrain_mud", "res://textures/terrain/mud.png", sbx::graphics::format::r8g8b8a8_srgb);

    sbx::utility::logger<"terrain">::info("Initialized {}x{} terrain (cell size: {}m, chunks: {}x{})", config.world_width, config.world_height, grid::cell_size, (config.world_width + chunk_size - 1) / chunk_size, (config.world_height + chunk_size - 1) / chunk_size);
  }

  ~terrain_module() override = default;

  auto update() -> void override { }

  auto grid() -> demo::grid& { 
    return _grid; 
  }

  auto grid() const -> const demo::grid& { 
    return _grid; 
  }

  auto heightmap() -> demo::heightmap& { 
    return _heightmap; 
  }

  auto heightmap() const -> const demo::heightmap& { 
    return _heightmap; 
  }

  auto get_splat_chunk(chunk_coord chunk_coordinates) -> splat_chunk* {
    auto entry = _splat_chunks.find(chunk_coordinates);

    return (entry != _splat_chunks.end()) ? &entry->second : nullptr;
  }

  auto get_splat_chunk(chunk_coord chunk_coordinates) const -> const splat_chunk* {
    auto entry = _splat_chunks.find(chunk_coordinates);

    return (entry != _splat_chunks.end()) ? &entry->second : nullptr;
  }

  auto ensure_splat_chunk(chunk_coord chunk_coordinates) -> splat_chunk& {
    auto entry = _splat_chunks.find(chunk_coordinates);

    if (entry != _splat_chunks.end()) {
      return entry->second;
    }

    _heightmap.ensure_chunk(chunk_coordinates);

    auto& splat = _splat_chunks[chunk_coordinates];
    generate_splat_for_chunk(splat, _heightmap, chunk_coordinates, config.splat);

    return splat;
  }

  auto regenerate_splat(chunk_coord chunk_coordinates) -> void {
    auto entry = _splat_chunks.find(chunk_coordinates);

    if (entry == _splat_chunks.end()) {
      return;
    }

    generate_splat_for_chunk(entry->second, _heightmap, chunk_coordinates, config.splat);
  }

  auto regenerate_splat_range(chunk_coord min_chunk, chunk_coord max_chunk) -> void {
    for (auto chunk_y = min_chunk.y; chunk_y <= max_chunk.y; ++chunk_y) {
      for (auto chunk_x = min_chunk.x; chunk_x <= max_chunk.x; ++chunk_x) {
        regenerate_splat({chunk_x, chunk_y});
      }
    }
  }

  auto offset_x() const -> std::float_t { return _grid.offset_x(); }

  auto offset_z() const -> std::float_t { return _grid.offset_z(); }

  auto cell_size() const -> std::float_t { return grid::cell_size; }

  auto world_to_cell(std::float_t world_x, std::float_t world_z) const -> chunk_coord {
    auto cell_x = static_cast<std::int32_t>((world_x + offset_x()) / cell_size());
    auto cell_y = static_cast<std::int32_t>((world_z + offset_z()) / cell_size());

    return chunk_coord{cell_x, cell_y};
  }

  auto cell_to_world(std::int32_t cell_x, std::int32_t cell_y) const -> std::pair<std::float_t, std::float_t> {
    auto world_x = static_cast<std::float_t>(cell_x) * cell_size() - offset_x();
    auto world_z = static_cast<std::float_t>(cell_y) * cell_size() - offset_z();

    return {world_x, world_z};
  }

  auto get_height_at(std::float_t world_x, std::float_t world_z) const -> std::float_t {
    return _heightmap.get_height_at(world_x, world_z);
  }

  auto get_slope_at(std::int32_t cell_x, std::int32_t cell_y) const -> std::float_t {
    return get_slope_cost(_heightmap, cell_x, cell_y);
  }

  auto sculpt(std::float_t world_x, std::float_t world_z, std::float_t radius, std::float_t strength) -> sculpt_result {
    auto result = sculpt_terrain(_heightmap, world_x, world_z, radius, strength);

    regenerate_splat_range(result.min_chunk, result.max_chunk);

    return result;
  }

  auto flatten(std::int32_t cell_x, std::int32_t cell_y, std::uint32_t size_w, std::uint32_t size_h) -> void {
    flatten_for_building(_heightmap, cell_x, cell_y, size_w, size_h);
  }

  auto can_build_at(std::int32_t cell_x, std::int32_t cell_y, std::uint32_t size_w, std::uint32_t size_h, std::float_t max_slope = 3.0f) const -> bool {
    for (auto y = cell_y; y < cell_y + static_cast<std::int32_t>(size_h); ++y) {
      for (auto x = cell_x; x < cell_x + static_cast<std::int32_t>(size_w); ++x) {
        if (!_grid.in_bounds(x, y)) {
          return false;
        }

        const auto& grid_cell = _grid.at(x, y);

        if (grid_cell.building_id != 0) {
          return false;
        }

        if (grid_cell.terrain_type == 3) {
          return false;
        }

        if (get_slope_cost(_heightmap, x, y) > max_slope) {
          return false;
        }
      }
    }

    return true;
  }

  auto get_visible_chunks(std::float_t camera_x, std::float_t camera_z, std::float_t view_distance) const -> std::vector<chunk_coord> {
    auto camera_chunk_x = static_cast<std::int32_t>(std::floor((camera_x + offset_x()) / (chunk_size * cell_size())));
    auto camera_chunk_z = static_cast<std::int32_t>(std::floor((camera_z + offset_z()) / (chunk_size * cell_size())));

    auto chunk_radius = static_cast<std::int32_t>(std::ceil(view_distance / (chunk_size * cell_size())));

    auto max_chunk_x = static_cast<std::int32_t>((config.world_width + chunk_size - 1) / chunk_size);
    auto max_chunk_y = static_cast<std::int32_t>((config.world_height + chunk_size - 1) / chunk_size);

    auto visible_chunks = std::vector<chunk_coord>{};

    for (auto chunk_y = camera_chunk_z - chunk_radius; chunk_y <= camera_chunk_z + chunk_radius; ++chunk_y) {
      for (auto chunk_x = camera_chunk_x - chunk_radius; chunk_x <= camera_chunk_x + chunk_radius; ++chunk_x) {

        if (chunk_x < 0 || chunk_y < 0 || chunk_x >= max_chunk_x || chunk_y >= max_chunk_y) {
          continue;
        }

        visible_chunks.push_back(chunk_coord{chunk_x, chunk_y});
      }
    }

    return visible_chunks;
  }

  auto ensure_chunks_around(std::float_t world_x, std::float_t world_z, std::float_t radius) -> void {
    auto visible_chunks = get_visible_chunks(world_x, world_z, radius);

    for (const auto& chunk_coordinates : visible_chunks) {
      _heightmap.ensure_chunk(chunk_coordinates);
      ensure_splat_chunk(chunk_coordinates);
    }
  }

private:

  demo::grid _grid;
  demo::heightmap _heightmap;
  std::unordered_map<chunk_coord, splat_chunk, chunk_coord_hash> _splat_chunks;

}; // class terrain_module

} // namespace demo

#endif // DEMO_TERRAIN_TERRAIN_MODULE_HPP_
