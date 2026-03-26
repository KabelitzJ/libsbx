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

inline auto compute_road_mask(const grid& terrain_grid, std::int32_t cell_x, std::int32_t cell_y) -> std::uint8_t {
  if (!terrain_grid.in_bounds(cell_x, cell_y)) {
    return 0;
  }

  auto& center_cell = terrain_grid.at(cell_x, cell_y);

  if (center_cell.road_type == 0) {
    return 0;
  }

  auto mask = std::uint8_t{0};

  for (auto direction = 0u; direction < 8u; ++direction) {
    auto neighbor_x = cell_x + road_direction_offset_x[direction];
    auto neighbor_y = cell_y + road_direction_offset_y[direction];

    if (!terrain_grid.in_bounds(neighbor_x, neighbor_y)) {
      continue;
    }

    if (terrain_grid.at(neighbor_x, neighbor_y).road_type != 0) {
      mask |= (1u << direction);
    }
  }

  return mask;
}

inline auto update_road_connectivity(grid& terrain_grid, std::int32_t cell_x, std::int32_t cell_y) -> void {
  if (terrain_grid.in_bounds(cell_x, cell_y)) {
    terrain_grid.at(cell_x, cell_y).road_mask = compute_road_mask(terrain_grid, cell_x, cell_y);
  }

  for (auto direction = 0u; direction < 8u; ++direction) {
    auto neighbor_x = cell_x + road_direction_offset_x[direction];
    auto neighbor_y = cell_y + road_direction_offset_y[direction];

    if (terrain_grid.in_bounds(neighbor_x, neighbor_y)) {
      terrain_grid.at(neighbor_x, neighbor_y).road_mask = compute_road_mask(terrain_grid, neighbor_x, neighbor_y);
    }
  }
}

inline auto cell_to_chunk(std::int32_t cell_x, std::int32_t cell_y) -> chunk_coord {
  return chunk_coord{
    static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(cell_x) / chunk_size)),
    static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(cell_y) / chunk_size))
  };
}

struct road_placement_result {
  std::vector<chunk_coord> placed_cells;
  std::unordered_set<chunk_coord, chunk_coord_hash> dirty_chunks;
}; // struct road_placement_result

// Place road along an explicit list of cells (from road_drawing path computation).
inline auto place_road_path(grid& terrain_grid, const std::vector<chunk_coord>& cells, road_type type) -> road_placement_result {
  auto result = road_placement_result{};

  for (const auto& cell : cells) {
    if (!terrain_grid.in_bounds(cell.x, cell.y)) {
      continue;
    }

    auto& current_cell = terrain_grid.at(cell.x, cell.y);

    if (current_cell.building_id != 0) {
      continue;
    }

    auto new_type = static_cast<std::uint8_t>(type);

    if (new_type > current_cell.road_type) {
      current_cell.road_type = new_type;
      result.placed_cells.push_back(cell);
      result.dirty_chunks.insert(cell_to_chunk(cell.x, cell.y));
    }
  }

  for (const auto& placed_cell : result.placed_cells) {
    update_road_connectivity(terrain_grid, placed_cell.x, placed_cell.y);

    for (auto direction = 0u; direction < 8u; ++direction) {
      auto neighbor_x = placed_cell.x + road_direction_offset_x[direction];
      auto neighbor_y = placed_cell.y + road_direction_offset_y[direction];

      result.dirty_chunks.insert(cell_to_chunk(neighbor_x, neighbor_y));
    }
  }

  return result;
}

// Legacy bresenham placement — kept for compatibility.
inline auto place_road_line(grid& terrain_grid, std::int32_t start_x, std::int32_t start_y, std::int32_t end_x, std::int32_t end_y, road_type type) -> road_placement_result {
  auto result = road_placement_result{};

  auto dx = std::abs(end_x - start_x);
  auto dy = std::abs(end_y - start_y);
  auto sx = (start_x < end_x) ? 1 : -1;
  auto sy = (start_y < end_y) ? 1 : -1;
  auto error = dx - dy;

  auto current_x = start_x;
  auto current_y = start_y;

  while (true) {
    if (terrain_grid.in_bounds(current_x, current_y)) {
      auto& current_cell = terrain_grid.at(current_x, current_y);

      if (current_cell.building_id == 0) {
        auto new_type = static_cast<std::uint8_t>(type);

        if (new_type > current_cell.road_type) {
          current_cell.road_type = new_type;
          result.placed_cells.push_back({current_x, current_y});
          result.dirty_chunks.insert(cell_to_chunk(current_x, current_y));
        }
      }
    }

    if (current_x == end_x && current_y == end_y) {
      break;
    }

    auto error2 = error * 2;

    if (error2 > -dy) {
      error -= dy;
      current_x += sx;
    }

    if (error2 < dx) {
      error += dx;
      current_y += sy;
    }
  }

  for (const auto& placed_cell : result.placed_cells) {
    update_road_connectivity(terrain_grid, placed_cell.x, placed_cell.y);

    for (auto direction = 0u; direction < 8u; ++direction) {
      auto neighbor_x = placed_cell.x + road_direction_offset_x[direction];
      auto neighbor_y = placed_cell.y + road_direction_offset_y[direction];

      result.dirty_chunks.insert(cell_to_chunk(neighbor_x, neighbor_y));
    }
  }

  return result;
}

inline auto remove_road(grid& terrain_grid, std::int32_t cell_x, std::int32_t cell_y) -> std::unordered_set<chunk_coord, chunk_coord_hash> {
  auto dirty_chunks = std::unordered_set<chunk_coord, chunk_coord_hash>{};

  if (!terrain_grid.in_bounds(cell_x, cell_y)) {
    return dirty_chunks;
  }

  auto& current_cell = terrain_grid.at(cell_x, cell_y);

  if (current_cell.road_type == 0) {
    return dirty_chunks;
  }

  current_cell.road_type = 0;
  current_cell.road_mask = 0;

  dirty_chunks.insert(cell_to_chunk(cell_x, cell_y));

  for (auto direction = 0u; direction < 8u; ++direction) {
    auto neighbor_x = cell_x + road_direction_offset_x[direction];
    auto neighbor_y = cell_y + road_direction_offset_y[direction];

    if (terrain_grid.in_bounds(neighbor_x, neighbor_y)) {
      terrain_grid.at(neighbor_x, neighbor_y).road_mask = compute_road_mask(terrain_grid, neighbor_x, neighbor_y);
      dirty_chunks.insert(cell_to_chunk(neighbor_x, neighbor_y));
    }
  }

  return dirty_chunks;
}

} // namespace demo

#endif // DEMO_BUILDING_ROAD_PLACEMENT_HPP_
