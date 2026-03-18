// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_SCULPT_HPP_
#define DEMO_TERRAIN_SCULPT_HPP_

#include <cstdint>
#include <cmath>
#include <algorithm>

#include <demo/terrain/heightmap.hpp>
#include <demo/terrain/grid.hpp>

namespace demo {

struct sculpt_result {
  std::int32_t min_vertex_x{};
  std::int32_t min_vertex_z{};
  std::int32_t max_vertex_x{};
  std::int32_t max_vertex_z{};
}; // struct sculpt_result

inline auto sculpt_terrain(heightmap& height_map, std::float_t world_x, std::float_t world_z, std::float_t radius, std::float_t strength) -> sculpt_result {
  auto cell = grid::cell_size;
  auto offset_x = static_cast<std::float_t>(height_map.world_width()) * cell * 0.5f;
  auto offset_z = static_cast<std::float_t>(height_map.world_height()) * cell * 0.5f;

  auto grid_x = world_x + offset_x;
  auto grid_z = world_z + offset_z;

  auto center_x = static_cast<std::int32_t>(grid_x / cell);
  auto center_z = static_cast<std::int32_t>(grid_z / cell);
  auto radius_cells = static_cast<std::int32_t>(std::ceil(radius / cell));

  auto result = sculpt_result{};
  result.min_vertex_x = std::max(center_x - radius_cells, 0);
  result.min_vertex_z = std::max(center_z - radius_cells, 0);
  result.max_vertex_x = std::min(center_x + radius_cells, static_cast<std::int32_t>(height_map.vertices_x() - 1));
  result.max_vertex_z = std::min(center_z + radius_cells, static_cast<std::int32_t>(height_map.vertices_z() - 1));

  for (auto vertex_z = result.min_vertex_z; vertex_z <= result.max_vertex_z; ++vertex_z) {
    for (auto vertex_x = result.min_vertex_x; vertex_x <= result.max_vertex_x; ++vertex_x) {
      auto delta_x = static_cast<std::float_t>(vertex_x) * cell - grid_x;
      auto delta_z = static_cast<std::float_t>(vertex_z) * cell - grid_z;
      auto distance = std::sqrt(delta_x * delta_x + delta_z * delta_z);

      if (distance < radius) {
        auto falloff = 1.0f - (distance / radius);
        falloff *= falloff;

        auto current = height_map.get_height(vertex_x, vertex_z);
        height_map.set_height(vertex_x, vertex_z, current + strength * falloff);
      }
    }
  }

  return result;
}

inline auto flatten_for_building(heightmap& height_map, std::int32_t cell_x, std::int32_t cell_z, std::uint32_t size_width, std::uint32_t size_height) -> void {
  auto average = 0.0f;
  auto count = 0u;

  for (auto vertex_z = cell_z; vertex_z <= cell_z + static_cast<std::int32_t>(size_height); ++vertex_z) {
    for (auto vertex_x = cell_x; vertex_x <= cell_x + static_cast<std::int32_t>(size_width); ++vertex_x) {
      average += height_map.get_height(vertex_x, vertex_z);
      ++count;
    }
  }

  average /= static_cast<std::float_t>(count);

  for (auto vertex_z = cell_z; vertex_z <= cell_z + static_cast<std::int32_t>(size_height); ++vertex_z) {
    for (auto vertex_x = cell_x; vertex_x <= cell_x + static_cast<std::int32_t>(size_width); ++vertex_x) {
      height_map.set_height(vertex_x, vertex_z, average);
    }
  }
}

inline auto get_slope_cost(const heightmap& height_map, std::int32_t cell_x, std::int32_t cell_z) -> std::float_t {
  auto h00 = height_map.get_height(cell_x, cell_z);
  auto h10 = height_map.get_height(cell_x + 1, cell_z);
  auto h01 = height_map.get_height(cell_x, cell_z + 1);
  auto h11 = height_map.get_height(cell_x + 1, cell_z + 1);

  auto max_diff = std::max({
    std::abs(h00 - h10),
    std::abs(h00 - h01),
    std::abs(h10 - h11),
    std::abs(h01 - h11),
    std::abs(h00 - h11),
    std::abs(h10 - h01)
  });

  constexpr auto slope_penalty = 2.0f;

  return max_diff * slope_penalty;
}

} // namespace demo

#endif // DEMO_TERRAIN_SCULPT_HPP_
