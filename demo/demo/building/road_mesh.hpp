// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDING_ROAD_MESH_HPP_
#define DEMO_BUILDING_ROAD_MESH_HPP_

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <array>
#include <utility>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/grid.hpp>
#include <demo/terrain/heightmap.hpp>

#include <demo/building/road_types.hpp>

namespace demo {

struct road_vertex {
  std::float_t position_x;
  std::float_t position_y;
  std::float_t position_z;
  std::float_t uv_u;
  std::float_t uv_v;
  std::uint32_t road_type;
}; // struct road_vertex

struct road_mesh_data {
  std::vector<road_vertex> vertices;
  std::vector<std::uint32_t> indices;
  bool is_empty{true};
}; // struct road_mesh_data

struct _edge_point {
  std::float_t angle;
  std::uint32_t vertex_index;
}; // struct _edge_point

inline auto _height_at(const heightmap& heightmap, std::float_t x, std::float_t z, std::float_t offset) -> std::float_t {
  return heightmap.get_height_at(x, z) + offset;
}

inline auto build_road_mesh() -> road_mesh_data {
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  auto mesh = road_mesh_data{};

  auto& grid = terrain_module.grid();
  auto& heightmap = terrain_module.heightmap();

  auto grid_w = static_cast<std::int32_t>(grid.world_width());
  auto grid_h = static_cast<std::int32_t>(grid.world_height());

  auto half_cell = grid::cell_size * 0.5f;

  auto cell_center = [&](std::int32_t gx, std::int32_t gz) -> std::pair<std::float_t, std::float_t> {
    auto [wx, wz] = terrain_module.cell_to_world(cell_coordinates{gx, gz});
    return {wx + half_cell, wz + half_cell};
  };

  for (auto gz = 0; gz < grid_h; ++gz) {
    for (auto gx = 0; gx < grid_w; ++gx) {
      if (!grid.in_bounds(gx, gz)) {
        continue;
      }

      auto& cell = grid.at(gx, gz);

      if (cell.road_type == 0) {
        continue;
      }

      auto props = get_road_properties(static_cast<road_type>(cell.road_type));
      auto half_w = props.half_width;

      auto [cx, cz] = cell_center(gx, gz);
      auto cy = _height_at(heightmap, cx, cz, props.height_offset);

      auto mask = cell.road_mask;

      auto connection_count = 0u;

      for (auto dir = 0u; dir < 8u; ++dir) {
        if ((mask & (1u << dir)) != 0) {
          ++connection_count;
        }
      }

      if (connection_count == 0) {
        constexpr auto cap_segments = 8u;
        auto cap_base = static_cast<std::uint32_t>(mesh.vertices.size());

        mesh.vertices.push_back(road_vertex{
          .position_x = cx, .position_y = cy, .position_z = cz,
          .uv_u = 0.5f, .uv_v = 0.5f,
          .road_type = static_cast<std::uint32_t>(cell.road_type),
        });

        for (auto s = 0u; s < cap_segments; ++s) {
          auto angle = static_cast<std::float_t>(s) * 3.14159265f * 2.0f / static_cast<std::float_t>(cap_segments);

          mesh.vertices.push_back(road_vertex{
            .position_x = cx + std::cos(angle) * half_w,
            .position_y = _height_at(heightmap, cx + std::cos(angle) * half_w, cz + std::sin(angle) * half_w, props.height_offset),
            .position_z = cz + std::sin(angle) * half_w,
            .uv_u = 0.5f + std::cos(angle) * 0.5f,
            .uv_v = 0.5f + std::sin(angle) * 0.5f,
            .road_type = static_cast<std::uint32_t>(cell.road_type),
          });
        }

        for (auto s = 0u; s < cap_segments; ++s) {
          mesh.indices.push_back(cap_base);
          mesh.indices.push_back(cap_base + 1 + s);
          mesh.indices.push_back(cap_base + 1 + (s + 1) % cap_segments);
        }

        continue;
      }

      auto center_index = static_cast<std::uint32_t>(mesh.vertices.size());

      mesh.vertices.push_back(road_vertex{
        .position_x = cx, .position_y = cy, .position_z = cz,
        .uv_u = 0.5f, .uv_v = 0.5f,
        .road_type = static_cast<std::uint32_t>(cell.road_type),
      });

      auto edge_points = std::vector<_edge_point>{};
      edge_points.reserve(connection_count * 2);

      auto dir_left_index = std::array<std::uint32_t, 8>{};
      auto dir_right_index = std::array<std::uint32_t, 8>{};

      for (auto dir = 0u; dir < 8u; ++dir) {
        if ((mask & (1u << dir)) == 0) {
          continue;
        }

        auto nx = gx + road_direction_offset_x[dir];
        auto nz = gz + road_direction_offset_z[dir];

        auto [nwx, nwz] = cell_center(nx, nz);

        auto dx = nwx - cx;
        auto dz = nwz - cz;
        auto dl = std::sqrt(dx * dx + dz * dz);

        if (dl < 0.001f) {
          continue;
        }

        dx /= dl;
        dz /= dl;

        auto px = -dz;
        auto pz = dx;

        // Left edge
        auto lx = cx - px * half_w;
        auto lz = cz - pz * half_w;
        auto ly = _height_at(heightmap, lx, lz, props.height_offset);
        auto left_idx = static_cast<std::uint32_t>(mesh.vertices.size());

        mesh.vertices.push_back(road_vertex{
          .position_x = lx, .position_y = ly, .position_z = lz,
          .uv_u = 0.0f, .uv_v = 0.0f,
          .road_type = static_cast<std::uint32_t>(cell.road_type),
        });

        edge_points.push_back(_edge_point{
          .angle = std::atan2(lz - cz, lx - cx),
          .vertex_index = left_idx,
        });

        // Right edge
        auto rx = cx + px * half_w;
        auto rz = cz + pz * half_w;
        auto ry = _height_at(heightmap, rx, rz, props.height_offset);
        auto right_idx = static_cast<std::uint32_t>(mesh.vertices.size());

        mesh.vertices.push_back(road_vertex{
          .position_x = rx, .position_y = ry, .position_z = rz,
          .uv_u = 1.0f, .uv_v = 0.0f,
          .road_type = static_cast<std::uint32_t>(cell.road_type),
        });

        edge_points.push_back(_edge_point{
          .angle = std::atan2(rz - cz, rx - cx),
          .vertex_index = right_idx,
        });

        dir_left_index[dir] = left_idx;
        dir_right_index[dir] = right_idx;
      }

      // Sort edge points by angle and fan-fill
      std::sort(edge_points.begin(), edge_points.end(), [](const _edge_point& a, const _edge_point& b) {
        return a.angle < b.angle;
      });

      auto edge_count = static_cast<std::uint32_t>(edge_points.size());

      // Emit dedicated fan ring with radial UVs (avoids degenerate UV determinant)
      auto fan_ring_base = static_cast<std::uint32_t>(mesh.vertices.size());

      for (auto i = 0u; i < edge_count; ++i) {
        auto& src = mesh.vertices[edge_points[i].vertex_index];

        mesh.vertices.push_back(road_vertex{
          .position_x = src.position_x, .position_y = src.position_y, .position_z = src.position_z,
          .uv_u = 0.5f + std::cos(edge_points[i].angle) * 0.5f,
          .uv_v = 0.5f + std::sin(edge_points[i].angle) * 0.5f,
          .road_type = src.road_type,
        });
      }

      for (auto i = 0u; i < edge_count; ++i) {
        auto next = (i + 1) % edge_count;

        mesh.indices.push_back(center_index);
        mesh.indices.push_back(fan_ring_base + i);
        mesh.indices.push_back(fan_ring_base + next);
      }

      // ---- Connection strips (deduped) ----

      for (auto dir = 0u; dir < 8u; ++dir) {
        if ((mask & (1u << dir)) == 0) {
          continue;
        }

        auto nx = gx + road_direction_offset_x[dir];
        auto nz = gz + road_direction_offset_z[dir];

        if (nx < gx || (nx == gx && nz < gz)) {
          continue;
        }

        if (!grid.in_bounds(nx, nz)) {
          continue;
        }

        auto& neighbor_cell = grid.at(nx, nz);

        if (neighbor_cell.road_type == 0) {
          continue;
        }

        auto neighbor_props = get_road_properties(static_cast<road_type>(neighbor_cell.road_type));
        auto neighbor_half_w = neighbor_props.half_width;

        auto [nwx, nwz] = cell_center(nx, nz);

        auto dx = nwx - cx;
        auto dz = nwz - cz;
        auto dl = std::sqrt(dx * dx + dz * dz);

        if (dl < 0.001f) {
          continue;
        }

        dx /= dl;
        dz /= dl;

        auto px = -dz;
        auto pz = dx;

        // Start: reuse shared edge point vertices
        auto start_left = dir_left_index[dir];
        auto start_right = dir_right_index[dir];

        // End: new vertices at neighbor center
        auto el_x = nwx - px * neighbor_half_w;
        auto el_z = nwz - pz * neighbor_half_w;
        auto el_y = _height_at(heightmap, el_x, el_z, neighbor_props.height_offset);
        auto end_left = static_cast<std::uint32_t>(mesh.vertices.size());

        mesh.vertices.push_back(road_vertex{
          .position_x = el_x, .position_y = el_y, .position_z = el_z,
          .uv_u = 0.0f, .uv_v = dl,
          .road_type = static_cast<std::uint32_t>(cell.road_type),
        });

        auto er_x = nwx + px * neighbor_half_w;
        auto er_z = nwz + pz * neighbor_half_w;
        auto er_y = _height_at(heightmap, er_x, er_z, neighbor_props.height_offset);
        auto end_right = static_cast<std::uint32_t>(mesh.vertices.size());

        mesh.vertices.push_back(road_vertex{
          .position_x = er_x, .position_y = er_y, .position_z = er_z,
          .uv_u = 1.0f, .uv_v = dl,
          .road_type = static_cast<std::uint32_t>(cell.road_type),
        });

        mesh.indices.push_back(start_left);
        mesh.indices.push_back(end_left);
        mesh.indices.push_back(start_right);

        mesh.indices.push_back(start_right);
        mesh.indices.push_back(end_left);
        mesh.indices.push_back(end_right);
      }
    }
  }

  mesh.is_empty = mesh.vertices.empty();

  return mesh;
}

inline auto build_road_preview_mesh(const std::vector<cell_coordinates>& cells, road_type type) -> road_mesh_data {
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  auto mesh = road_mesh_data{};

  if (cells.empty()) {
    return mesh;
  }

  auto& heightmap = terrain_module.heightmap();

  auto props = get_road_properties(type);
  auto half = grid::cell_size * 0.5f;
  auto road_type_val = static_cast<std::uint32_t>(type);
  auto h_offset = props.height_offset + 0.02f;

  for (const auto& cell : cells) {
    auto [wx, wz] = terrain_module.cell_to_world(cell);
    auto cx = wx + half;
    auto cz = wz + half;

    auto h00 = _height_at(heightmap, cx - half, cz - half, h_offset);
    auto h10 = _height_at(heightmap, cx + half, cz - half, h_offset);
    auto h11 = _height_at(heightmap, cx + half, cz + half, h_offset);
    auto h01 = _height_at(heightmap, cx - half, cz + half, h_offset);

    auto base = static_cast<std::uint32_t>(mesh.vertices.size());

    mesh.vertices.push_back({cx - half, h00, cz - half, 0.0f, 0.0f, road_type_val});
    mesh.vertices.push_back({cx + half, h10, cz - half, 1.0f, 0.0f, road_type_val});
    mesh.vertices.push_back({cx + half, h11, cz + half, 1.0f, 1.0f, road_type_val});
    mesh.vertices.push_back({cx - half, h01, cz + half, 0.0f, 1.0f, road_type_val});

    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);

    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
  }

  mesh.is_empty = mesh.vertices.empty();

  return mesh;
}

} // namespace demo

#endif // DEMO_BUILDING_ROAD_MESH_HPP_
