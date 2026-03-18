// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_TERRAIN_MODULE_HPP_
#define DEMO_TERRAIN_TERRAIN_MODULE_HPP_

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
}; // struct terrain_config

class terrain_module final : public sbx::core::module<terrain_module> {

  inline static const auto is_registered = register_module(stage::normal);

  inline static const auto config = terrain_config{
    .world_width = 1024,
    .world_height = 1024,
    .generation = {
      .base_scale = 0.0015f,
      .height_scale = 30.0f,
      .warp_scale = 0.002f,
      .warp_strength = 80.0f,
      .octaves = 4u,
      .seed_x = 4809324.512381f,
      .seed_z = -3094852.123123f,
    },
    .splat = {
      .variety_scale = 0.05f,
      .variety_strength = 0.15f,
      .moisture_scale = 0.02f,
      .seed = 5432.193581f,
    },
  };

public:

  terrain_module()
  : _grid{config.world_width, config.world_height},
    _heightmap{config.world_width, config.world_height, config.generation},
    _splat_weights{generate_splat_map(_heightmap, config.splat)},
    _splat_dirty{true} {
    sbx::utility::logger<"terrain">::info("Initialized {}x{} terrain (cell size: {}m)", config.world_width, config.world_height, grid::cell_size);
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

  auto splat_data() const -> const splat_weights* {
    return _splat_weights.data();
  }

  auto splat_data_size_bytes() const -> std::size_t {
    return _splat_weights.size() * sizeof(splat_weights);
  }

  auto is_splat_dirty() const -> bool {
    return _splat_dirty;
  }

  auto clear_splat_dirty() -> void {
    _splat_dirty = false;
  }

  auto offset_x() const -> std::float_t {
    return _grid.offset_x();
  }

  auto offset_z() const -> std::float_t {
    return _grid.offset_z();
  }

  auto cell_size() const -> std::float_t {
    return grid::cell_size;
  }

  auto world_to_cell(std::float_t world_x, std::float_t world_z) const -> chunk_coord {
    auto cell_x = static_cast<std::int32_t>((world_x + offset_x()) / cell_size());
    auto cell_z = static_cast<std::int32_t>((world_z + offset_z()) / cell_size());

    return chunk_coord{cell_x, cell_z};
  }

  auto cell_to_world(std::int32_t cell_x, std::int32_t cell_z) const -> std::pair<std::float_t, std::float_t> {
    auto world_x = static_cast<std::float_t>(cell_x) * cell_size() - offset_x();
    auto world_z = static_cast<std::float_t>(cell_z) * cell_size() - offset_z();

    return {world_x, world_z};
  }

  auto get_height_at(std::float_t world_x, std::float_t world_z) const -> std::float_t {
    return _heightmap.get_height_at(world_x, world_z);
  }

  auto get_slope_at(std::int32_t cell_x, std::int32_t cell_z) const -> std::float_t {
    return get_slope_cost(_heightmap, cell_x, cell_z);
  }

  auto sculpt(std::float_t world_x, std::float_t world_z, std::float_t radius, std::float_t strength) -> sculpt_result {
    auto result = sculpt_terrain(_heightmap, world_x, world_z, radius, strength);

    regenerate_splat_region(_splat_weights, _heightmap, result.min_vertex_x, result.min_vertex_z, result.max_vertex_x, result.max_vertex_z, config.splat);
    _splat_dirty = true;

    return result;
  }

  auto flatten(std::int32_t cell_x, std::int32_t cell_z, std::uint32_t size_width, std::uint32_t size_height) -> void {
    flatten_for_building(_heightmap, cell_x, cell_z, size_width, size_height);
  }

  auto get_height_at_cell(std::int32_t cell_x, std::int32_t cell_z) const -> std::float_t {
    auto [world_x, world_z] = cell_to_world(cell_x, cell_z);

    return _heightmap.get_height_at(world_x, world_z);
  }

  auto can_build_at(std::int32_t cell_x, std::int32_t cell_z, std::uint32_t size_width, std::uint32_t size_height, std::float_t max_slope = 3.0f) const -> bool {
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

  auto world_width() const -> std::uint32_t {
    return config.world_width;
  }

  auto world_height() const -> std::uint32_t {
    return config.world_height;
  }

private:

  demo::grid _grid;
  demo::heightmap _heightmap;
  std::vector<splat_weights> _splat_weights;
  bool _splat_dirty{true};

}; // class terrain_module

} // namespace demo

#endif // DEMO_TERRAIN_TERRAIN_MODULE_HPP_
