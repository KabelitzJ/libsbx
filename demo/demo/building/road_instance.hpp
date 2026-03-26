// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDING_ROAD_INSTANCE_HPP_
#define DEMO_BUILDING_ROAD_INSTANCE_HPP_

#include <vector>
#include <cstdint>
#include <cmath>

#include <demo/terrain/grid.hpp>
#include <demo/terrain/heightmap.hpp>

#include <demo/building/road_types.hpp>

namespace demo {

struct road_instance {
  std::float_t position_x;
  std::float_t position_y;
  std::float_t position_z;
  std::uint32_t packed_data; // road_type [0:7] | road_mask [8:15]
}; // struct road_instance

inline auto pack_road_data(std::uint8_t road_type, std::uint8_t road_mask) -> std::uint32_t {
  return static_cast<std::uint32_t>(road_type) | (static_cast<std::uint32_t>(road_mask) << 8u);
}

struct road_instance_data {
  std::vector<road_instance> instances;
  bool is_empty{true};
}; // struct road_instance_data

inline auto build_road_instances(const grid& terrain_grid, const heightmap& height_map) -> road_instance_data {
  auto data = road_instance_data{};

  auto grid_w = terrain_grid.world_width();
  auto grid_h = terrain_grid.world_height();

  auto terrain_offset_x = static_cast<std::float_t>(grid_w) * grid::cell_size * 0.5f;
  auto terrain_offset_z = static_cast<std::float_t>(grid_h) * grid::cell_size * 0.5f;

  for (auto gy = 0; gy < grid_h; ++gy) {
    for (auto gx = 0; gx < grid_w; ++gx) {
      if (!terrain_grid.in_bounds(gx, gy)) {
        continue;
      }

      auto& cell = terrain_grid.at(gx, gy);

      if (cell.road_type == 0) {
        continue;
      }

      auto world_x = (static_cast<std::float_t>(gx) + 0.5f) * grid::cell_size - terrain_offset_x;
      auto world_z = (static_cast<std::float_t>(gy) + 0.5f) * grid::cell_size - terrain_offset_z;

      auto properties = get_road_properties(static_cast<road_type>(cell.road_type));
      auto world_y = height_map.get_height_at(world_x, world_z) + properties.height_offset;

      data.instances.push_back(road_instance{
        .position_x = world_x,
        .position_y = world_y,
        .position_z = world_z,
        .packed_data = pack_road_data(cell.road_type, cell.road_mask),
      });
    }
  }

  data.is_empty = data.instances.empty();

  return data;
}

} // namespace demo

#endif // DEMO_BUILDING_ROAD_INSTANCE_HPP_
