// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDING_ROAD_PLACEMENT_HPP_
#define DEMO_BUILDING_ROAD_PLACEMENT_HPP_

#include <vector>
#include <cstdint>
#include <cmath>
#include <unordered_set>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/grid.hpp>

#include <demo/building/road_types.hpp>

namespace demo {

inline auto compute_road_mask(const grid& terrain_grid, std::int32_t cell_x, std::int32_t cell_z) -> std::uint8_t {
  if (!terrain_grid.in_bounds(cell_x, cell_z)) {
    return 0;
  }

  auto& center_cell = terrain_grid.at(cell_x, cell_z);

  if (center_cell.road_type == 0) {
    return 0;
  }

  auto mask = std::uint8_t{0};

  for (auto direction = 0u; direction < 8u; ++direction) {
    auto neighbor_x = cell_x + road_direction_offset_x[direction];
    auto neighbor_z = cell_z + road_direction_offset_z[direction];

    if (!terrain_grid.in_bounds(neighbor_x, neighbor_z)) {
      continue;
    }

    if (terrain_grid.at(neighbor_x, neighbor_z).road_type != 0) {
      mask |= (1u << direction);
    }
  }

  return mask;
}

inline auto update_road_connectivity(grid& terrain_grid, std::int32_t cell_x, std::int32_t cell_z) -> void {
  if (terrain_grid.in_bounds(cell_x, cell_z)) {
    terrain_grid.at(cell_x, cell_z).road_mask = compute_road_mask(terrain_grid, cell_x, cell_z);
  }

  for (auto direction = 0u; direction < 8u; ++direction) {
    auto neighbor_x = cell_x + road_direction_offset_x[direction];
    auto neighbor_z = cell_z + road_direction_offset_z[direction];

    if (terrain_grid.in_bounds(neighbor_x, neighbor_z)) {
      terrain_grid.at(neighbor_x, neighbor_z).road_mask = compute_road_mask(terrain_grid, neighbor_x, neighbor_z);
    }
  }
}

inline auto cell_to_chunk(std::int32_t cell_x, std::int32_t cell_z) -> cell_coordinates {
  return cell_coordinates{
    static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(cell_x) / chunk_size)),
    static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(cell_z) / chunk_size))
  };
}

struct road_placement_result {
  std::vector<cell_coordinates> placed_cells;
  std::unordered_set<cell_coordinates, cell_coordinates_hash> dirty_chunks;
}; // struct road_placement_result

// Place road along an explicit list of cells (from road_drawing path computation).
inline auto place_road_path(grid& terrain_grid, const std::vector<cell_coordinates>& cells, road_type type) -> road_placement_result {
  auto result = road_placement_result{};

  for (const auto& cell : cells) {
    if (!terrain_grid.in_bounds(cell.x, cell.z)) {
      continue;
    }

    auto& current_cell = terrain_grid.at(cell.x, cell.z);

    if (current_cell.building_id != 0) {
      continue;
    }

    auto new_type = static_cast<std::uint8_t>(type);

    if (new_type > current_cell.road_type) {
      current_cell.road_type = new_type;
      result.placed_cells.push_back(cell);
      result.dirty_chunks.insert(cell_to_chunk(cell.x, cell.z));
    }
  }

  for (const auto& placed_cell : result.placed_cells) {
    update_road_connectivity(terrain_grid, placed_cell.x, placed_cell.z);

    for (auto direction = 0u; direction < 8u; ++direction) {
      auto neighbor_x = placed_cell.x + road_direction_offset_x[direction];
      auto neighbor_z = placed_cell.z + road_direction_offset_z[direction];

      result.dirty_chunks.insert(cell_to_chunk(neighbor_x, neighbor_z));
    }
  }

  return result;
}

// Legacy bresenham placement — kept for compatibility.
inline auto place_road_line(grid& terrain_grid, std::int32_t start_x, std::int32_t start_z, std::int32_t end_x, std::int32_t end_z, road_type type) -> road_placement_result {
  auto result = road_placement_result{};

  auto dx = std::abs(end_x - start_x);
  auto dz = std::abs(end_z - start_z);
  auto sx = (start_x < end_x) ? 1 : -1;
  auto sz = (start_z < end_z) ? 1 : -1;
  auto error = dx - dz;

  auto current_x = start_x;
  auto current_z = start_z;

  while (true) {
    if (terrain_grid.in_bounds(current_x, current_z)) {
      auto& current_cell = terrain_grid.at(current_x, current_z);

      if (current_cell.building_id == 0) {
        auto new_type = static_cast<std::uint8_t>(type);

        if (new_type > current_cell.road_type) {
          current_cell.road_type = new_type;
          result.placed_cells.push_back({current_x, current_z});
          result.dirty_chunks.insert(cell_to_chunk(current_x, current_z));
        }
      }
    }

    if (current_x == end_x && current_z == end_z) {
      break;
    }

    auto error2 = error * 2;

    if (error2 > -dz) {
      error -= dz;
      current_x += sx;
    }

    if (error2 < dx) {
      error += dx;
      current_z += sz;
    }
  }

  for (const auto& placed_cell : result.placed_cells) {
    update_road_connectivity(terrain_grid, placed_cell.x, placed_cell.z);

    for (auto direction = 0u; direction < 8u; ++direction) {
      auto neighbor_x = placed_cell.x + road_direction_offset_x[direction];
      auto neighbor_z = placed_cell.z + road_direction_offset_z[direction];

      result.dirty_chunks.insert(cell_to_chunk(neighbor_x, neighbor_z));
    }
  }

  return result;
}

inline auto remove_road(grid& terrain_grid, std::int32_t cell_x, std::int32_t cell_z) -> std::unordered_set<cell_coordinates, cell_coordinates_hash> {
  auto dirty_chunks = std::unordered_set<cell_coordinates, cell_coordinates_hash>{};

  if (!terrain_grid.in_bounds(cell_x, cell_z)) {
    return dirty_chunks;
  }

  auto& current_cell = terrain_grid.at(cell_x, cell_z);

  if (current_cell.road_type == 0) {
    return dirty_chunks;
  }

  current_cell.road_type = 0;
  current_cell.road_mask = 0;

  dirty_chunks.insert(cell_to_chunk(cell_x, cell_z));

  for (auto direction = 0u; direction < 8u; ++direction) {
    auto neighbor_x = cell_x + road_direction_offset_x[direction];
    auto neighbor_z = cell_z + road_direction_offset_z[direction];

    if (terrain_grid.in_bounds(neighbor_x, neighbor_z)) {
      terrain_grid.at(neighbor_x, neighbor_z).road_mask = compute_road_mask(terrain_grid, neighbor_x, neighbor_z);
      dirty_chunks.insert(cell_to_chunk(neighbor_x, neighbor_z));
    }
  }

  return dirty_chunks;
}

} // namespace demo

#endif // DEMO_BUILDING_ROAD_PLACEMENT_HPP_
