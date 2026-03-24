// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDING_ROAD_MESH_HPP_
#define DEMO_BUILDING_ROAD_MESH_HPP_

#include <vector>
#include <cstdint>
#include <cmath>

#include <libsbx/math/noise.hpp>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/grid.hpp>
#include <demo/terrain/heightmap.hpp>

#include <demo/building/road_types.hpp>

namespace demo {

struct road_vertex {
  std::float_t position_x;
  std::float_t position_y;
  std::float_t position_z;
  std::float_t edge_factor;
  std::uint32_t road_type;
}; // struct road_vertex

struct road_chunk_mesh {
  std::vector<road_vertex> vertices;
  std::vector<std::uint32_t> indices;
  bool is_empty{true};
}; // struct road_chunk_mesh

inline auto _road_world_height(const heightmap& height_map, std::float_t world_x, std::float_t world_z, std::float_t height_offset) -> std::float_t {
  return height_map.get_height_at(world_x, world_z) + height_offset;
}

struct cross_section {
  std::float_t x;
  std::float_t z;
  std::float_t local_half_width;
}; // struct cross_section 

inline auto build_road_chunk_mesh(const grid& terrain_grid, const heightmap& height_map, chunk_coord chunk_coordinates, std::float_t seed = 0.0f) -> road_chunk_mesh {
  auto mesh = road_chunk_mesh{};

  auto base_x = chunk_coordinates.x * static_cast<std::int32_t>(chunk_size);
  auto base_y = chunk_coordinates.y * static_cast<std::int32_t>(chunk_size);

  auto terrain_offset_x = static_cast<std::float_t>(terrain_grid.world_width()) * grid::cell_size * 0.5f;
  auto terrain_offset_z = static_cast<std::float_t>(terrain_grid.world_height()) * grid::cell_size * 0.5f;

  for (auto local_y = 0u; local_y < chunk_size; ++local_y) {
    for (auto local_x = 0u; local_x < chunk_size; ++local_x) {
      auto global_x = base_x + static_cast<std::int32_t>(local_x);
      auto global_y = base_y + static_cast<std::int32_t>(local_y);

      if (!terrain_grid.in_bounds(global_x, global_y)) {
        continue;
      }

      auto& current_cell = terrain_grid.at(global_x, global_y);

      if (current_cell.road_type == 0) {
        continue;
      }

      auto properties = get_road_properties(static_cast<road_type>(current_cell.road_type));

      auto cell_world_x = (static_cast<std::float_t>(global_x) + 0.5f) * grid::cell_size - terrain_offset_x;
      auto cell_world_z = (static_cast<std::float_t>(global_y) + 0.5f) * grid::cell_size - terrain_offset_z;

      auto gx = static_cast<std::float_t>(global_x);
      auto gz = static_cast<std::float_t>(global_y);

      auto jitter_x = sbx::math::noise::simplex(gx * 3.7f + seed, gz * 3.7f) * grid::cell_size * 0.06f;
      auto jitter_z = sbx::math::noise::simplex(gx * 5.1f, gz * 5.1f + seed) * grid::cell_size * 0.06f;

      auto center_x = cell_world_x + jitter_x;
      auto center_z = cell_world_z + jitter_z;
      auto center_y = _road_world_height(height_map, center_x, center_z, properties.height_offset);

      auto half_width = properties.half_width;

      auto width_noise = sbx::math::noise::simplex(gx * 2.3f + seed, gz * 2.3f + seed) * 0.05f;
      half_width *= (1.0f + width_noise);

      auto road_mask = current_cell.road_mask;

      auto vertex_base = static_cast<std::uint32_t>(mesh.vertices.size());

      mesh.vertices.push_back(road_vertex{
        .position_x = center_x,
        .position_y = center_y,
        .position_z = center_z,
        .edge_factor = 0.0f,
        .road_type = static_cast<std::uint32_t>(current_cell.road_type),
      });

      constexpr auto edge_vertex_count = 8u;

      for (auto direction = 0u; direction < edge_vertex_count; ++direction) {
        auto angle = static_cast<std::float_t>(direction) * 3.14159265f * 2.0f / static_cast<std::float_t>(edge_vertex_count);

        auto edge_x = center_x + std::cos(angle) * half_width;
        auto edge_z = center_z + std::sin(angle) * half_width;
        auto edge_y = _road_world_height(height_map, edge_x, edge_z, properties.height_offset);

        mesh.vertices.push_back(road_vertex{
          .position_x = edge_x,
          .position_y = edge_y,
          .position_z = edge_z,
          .edge_factor = 1.0f,
          .road_type = static_cast<std::uint32_t>(current_cell.road_type),
        });
      }

      for (auto direction = 0u; direction < edge_vertex_count; ++direction) {
        auto next_direction = (direction + 1) % edge_vertex_count;

        mesh.indices.push_back(vertex_base);
        mesh.indices.push_back(vertex_base + 1 + direction);
        mesh.indices.push_back(vertex_base + 1 + next_direction);
      }

      for (auto direction = 0u; direction < 8u; ++direction) {
        if ((road_mask & (1u << direction)) == 0) {
          continue;
        }

        auto neighbor_x = global_x + road_direction_offset_x[direction];
        auto neighbor_y = global_y + road_direction_offset_y[direction];

        if (neighbor_x < global_x || (neighbor_x == global_x && neighbor_y < global_y)) {
          continue;
        }

        if (!terrain_grid.in_bounds(neighbor_x, neighbor_y)) {
          continue;
        }

        auto neighbor_world_x = (static_cast<std::float_t>(neighbor_x) + 0.5f) * grid::cell_size - terrain_offset_x;
        auto neighbor_world_z = (static_cast<std::float_t>(neighbor_y) + 0.5f) * grid::cell_size - terrain_offset_z;

        auto neighbor_jitter_x = sbx::math::noise::simplex(static_cast<std::float_t>(neighbor_x) * 3.7f + seed, static_cast<std::float_t>(neighbor_y) * 3.7f) * grid::cell_size * 0.06f;
        auto neighbor_jitter_z = sbx::math::noise::simplex(static_cast<std::float_t>(neighbor_x) * 5.1f, static_cast<std::float_t>(neighbor_y) * 5.1f + seed) * grid::cell_size * 0.06f;

        neighbor_world_x += neighbor_jitter_x;
        neighbor_world_z += neighbor_jitter_z;

        auto direction_x = neighbor_world_x - center_x;
        auto direction_z = neighbor_world_z - center_z;
        auto direction_length = std::sqrt(direction_x * direction_x + direction_z * direction_z);

        if (direction_length < 0.001f) {
          continue;
        }

        direction_x /= direction_length;
        direction_z /= direction_length;

        auto perpendicular_x = -direction_z;
        auto perpendicular_z = direction_x;

        auto midpoint_x = (center_x + neighbor_world_x) * 0.5f;
        auto midpoint_z = (center_z + neighbor_world_z) * 0.5f;

        auto curve_noise = sbx::math::noise::simplex(midpoint_x * 1.7f + seed, midpoint_z * 1.7f + seed) * grid::cell_size * 0.05f;
        midpoint_x += perpendicular_x * curve_noise;
        midpoint_z += perpendicular_z * curve_noise;

        auto neighbor_road_type = static_cast<road_type>(terrain_grid.at(neighbor_x, neighbor_y).road_type);
        auto neighbor_properties = get_road_properties(neighbor_road_type);
        auto neighbor_half_width = neighbor_properties.half_width * (1.0f + width_noise);
        auto midpoint_half_width = (half_width + neighbor_half_width) * 0.5f;

        auto strip_base = static_cast<std::uint32_t>(mesh.vertices.size());

        auto sections = std::array<cross_section, 3>{
          cross_section{center_x, center_z, half_width},
          cross_section{midpoint_x, midpoint_z, midpoint_half_width},
          cross_section{neighbor_world_x, neighbor_world_z, neighbor_half_width},
        };

        for (const auto& section : sections) {
          auto section_y = _road_world_height(height_map, section.x, section.z, properties.height_offset);

          auto left_x = section.x - perpendicular_x * section.local_half_width;
          auto left_z = section.z - perpendicular_z * section.local_half_width;

          mesh.vertices.push_back(road_vertex{
            .position_x = left_x,
            .position_y = _road_world_height(height_map, left_x, left_z, properties.height_offset),
            .position_z = left_z,
            .edge_factor = 1.0f,
            .road_type = static_cast<std::uint32_t>(current_cell.road_type),
          });

          mesh.vertices.push_back(road_vertex{
            .position_x = section.x,
            .position_y = section_y,
            .position_z = section.z,
            .edge_factor = 0.0f,
            .road_type = static_cast<std::uint32_t>(current_cell.road_type),
          });

          auto right_x = section.x + perpendicular_x * section.local_half_width;
          auto right_z = section.z + perpendicular_z * section.local_half_width;

          mesh.vertices.push_back(road_vertex{
            .position_x = right_x,
            .position_y = _road_world_height(height_map, right_x, right_z, properties.height_offset),
            .position_z = right_z,
            .edge_factor = 1.0f,
            .road_type = static_cast<std::uint32_t>(current_cell.road_type),
          });
        }

        for (auto section_index = 0u; section_index < 2u; ++section_index) {
          auto base = strip_base + section_index * 3;

          mesh.indices.push_back(base + 0);
          mesh.indices.push_back(base + 3);
          mesh.indices.push_back(base + 1);

          mesh.indices.push_back(base + 1);
          mesh.indices.push_back(base + 3);
          mesh.indices.push_back(base + 4);

          mesh.indices.push_back(base + 1);
          mesh.indices.push_back(base + 4);
          mesh.indices.push_back(base + 2);

          mesh.indices.push_back(base + 2);
          mesh.indices.push_back(base + 4);
          mesh.indices.push_back(base + 5);
        }
      }
    }
  }

  mesh.is_empty = mesh.vertices.empty();

  return mesh;
}

} // namespace demo

#endif // DEMO_BUILDING_ROAD_MESH_HPP_
