// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDING_ZONE_OVERLAY_HPP_
#define DEMO_BUILDING_ZONE_OVERLAY_HPP_

#include <vector>
#include <cstdint>
#include <cmath>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/grid.hpp>
#include <demo/terrain/heightmap.hpp>

#include <demo/building/zone_types.hpp>

namespace demo {

struct zone_vertex {
  std::float_t position_x;
  std::float_t position_y;
  std::float_t position_z;
  std::float_t uv_u;
  std::float_t uv_v;
  std::uint32_t zone_type_val;
}; // struct zone_vertex

struct zone_mesh_data {
  std::vector<zone_vertex> vertices;
  std::vector<std::uint32_t> indices;
  bool is_empty{true};
}; // struct zone_mesh_data

static constexpr auto zone_height_offset = 0.06f;

// CellToWorldFn: (cell_coordinates) -> world_coordinates
template<typename CellToWorldFn>
auto build_zone_overlay_mesh(const grid& terrain_grid, const heightmap& height_map, CellToWorldFn&& cell_to_world) -> zone_mesh_data {
  auto mesh = zone_mesh_data{};

  auto grid_w = static_cast<std::int32_t>(terrain_grid.world_width());
  auto grid_h = static_cast<std::int32_t>(terrain_grid.world_height());

  auto cell_sz = grid::cell_size;

  for (auto gz = 0; gz < grid_h; ++gz) {
    for (auto gx = 0; gx < grid_w; ++gx) {
      if (!terrain_grid.in_bounds(gx, gz)) {
        continue;
      }

      auto& cell = terrain_grid.at(gx, gz);

      if (cell.zone_type == 0) {
        continue;
      }

      auto [wx, wz] = cell_to_world(cell_coordinates{gx, gz});

      auto h00 = height_map.get_height_at(wx, wz) + zone_height_offset;
      auto h10 = height_map.get_height_at(wx + cell_sz, wz) + zone_height_offset;
      auto h11 = height_map.get_height_at(wx + cell_sz, wz + cell_sz) + zone_height_offset;
      auto h01 = height_map.get_height_at(wx, wz + cell_sz) + zone_height_offset;

      auto zt = static_cast<std::uint32_t>(cell.zone_type);

      auto base = static_cast<std::uint32_t>(mesh.vertices.size());

      mesh.vertices.push_back({wx, h00, wz, 0.0f, 0.0f, zt});
      mesh.vertices.push_back({wx + cell_sz, h10, wz, 1.0f, 0.0f, zt});
      mesh.vertices.push_back({wx + cell_sz, h11, wz + cell_sz, 1.0f, 1.0f, zt});
      mesh.vertices.push_back({wx, h01, wz + cell_sz, 0.0f, 1.0f, zt});

      mesh.indices.push_back(base + 0);
      mesh.indices.push_back(base + 1);
      mesh.indices.push_back(base + 2);

      mesh.indices.push_back(base + 0);
      mesh.indices.push_back(base + 2);
      mesh.indices.push_back(base + 3);
    }
  }

  mesh.is_empty = mesh.vertices.empty();

  return mesh;
}

} // namespace demo

#endif // DEMO_BUILDING_ZONE_OVERLAY_HPP_
