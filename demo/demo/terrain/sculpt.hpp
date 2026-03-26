// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_SCULPT_HPP_
#define DEMO_TERRAIN_SCULPT_HPP_

#include <cstdint>
#include <cmath>
#include <algorithm>

#include <demo/terrain/heightmap.hpp>
#include <demo/terrain/grid.hpp>

namespace demo {

enum class sculpt_tool : std::uint8_t {
  raise,
  lower,
  flatten,
  smooth,
  level,
}; // enum class sculpt_tool

struct sculpt_result {
  std::int32_t min_vertex_x{};
  std::int32_t min_vertex_z{};
  std::int32_t max_vertex_x{};
  std::int32_t max_vertex_z{};
}; // struct sculpt_result

// Check if a vertex is protected by a nearby building or road.
// A vertex at (vx, vz) is shared by cells (vx-1, vz-1), (vx, vz-1), (vx-1, vz), (vx, vz).
// We also check a guard band around those cells.
inline auto _is_vertex_protected(const grid& terrain_grid, std::int32_t vertex_x, std::int32_t vertex_z, std::int32_t guard_radius = 1) -> bool {
  // The vertex touches up to 4 cells. Check those plus a guard radius.
  auto min_cx = vertex_x - 1 - guard_radius;
  auto max_cx = vertex_x + guard_radius;
  auto min_cz = vertex_z - 1 - guard_radius;
  auto max_cz = vertex_z + guard_radius;

  for (auto cz = min_cz; cz <= max_cz; ++cz) {
    for (auto cx = min_cx; cx <= max_cx; ++cx) {
      if (!terrain_grid.in_bounds(cx, cz)) {
        continue;
      }

      auto& c = terrain_grid.at(cx, cz);

      if (c.building_id != 0 || c.road_type != 0) {
        return true;
      }
    }
  }

  return false;
}

// Compute the bounding region of vertices affected by a world-space brush.
inline auto _compute_brush_bounds(const heightmap& height_map, std::float_t world_x, std::float_t world_z, std::float_t radius) -> sculpt_result {
  auto cell = grid::cell_size;
  auto offset_x = static_cast<std::float_t>(height_map.world_width()) * cell * 0.5f;
  auto offset_z = static_cast<std::float_t>(height_map.world_height()) * cell * 0.5f;

  auto grid_x = world_x + offset_x;
  auto grid_z = world_z + offset_z;

  auto center_x = static_cast<std::int32_t>(grid_x / cell);
  auto center_z = static_cast<std::int32_t>(grid_z / cell);
  auto radius_cells = static_cast<std::int32_t>(std::ceil(radius / cell));

  return sculpt_result{
    .min_vertex_x = std::max(center_x - radius_cells, 0),
    .min_vertex_z = std::max(center_z - radius_cells, 0),
    .max_vertex_x = std::min(center_x + radius_cells, static_cast<std::int32_t>(height_map.vertices_x() - 1)),
    .max_vertex_z = std::min(center_z + radius_cells, static_cast<std::int32_t>(height_map.vertices_z() - 1)),
  };
}

// Compute radial falloff from brush center. Returns 0..1, or -1 if outside radius.
inline auto _brush_falloff(std::int32_t vertex_x, std::int32_t vertex_z, std::float_t world_x, std::float_t world_z, std::float_t radius, const heightmap& height_map) -> std::float_t {
  auto cell = grid::cell_size;
  auto offset_x = static_cast<std::float_t>(height_map.world_width()) * cell * 0.5f;
  auto offset_z = static_cast<std::float_t>(height_map.world_height()) * cell * 0.5f;

  auto delta_x = static_cast<std::float_t>(vertex_x) * cell - (world_x + offset_x);
  auto delta_z = static_cast<std::float_t>(vertex_z) * cell - (world_z + offset_z);
  auto distance = std::sqrt(delta_x * delta_x + delta_z * delta_z);

  if (distance >= radius) {
    return -1.0f;
  }

  auto t = 1.0f - (distance / radius);

  return t * t;
}

// Raise or lower terrain with building/road protection.
inline auto sculpt_terrain(heightmap& height_map, const grid& terrain_grid, std::float_t world_x, std::float_t world_z, std::float_t radius, std::float_t strength, std::int32_t guard_radius = 1) -> sculpt_result {
  auto result = _compute_brush_bounds(height_map, world_x, world_z, radius);

  for (auto vz = result.min_vertex_z; vz <= result.max_vertex_z; ++vz) {
    for (auto vx = result.min_vertex_x; vx <= result.max_vertex_x; ++vx) {
      if (_is_vertex_protected(terrain_grid, vx, vz, guard_radius)) {
        continue;
      }

      auto falloff = _brush_falloff(vx, vz, world_x, world_z, radius, height_map);

      if (falloff < 0.0f) {
        continue;
      }

      auto current = height_map.get_height(vx, vz);
      height_map.set_height(vx, vz, current + strength * falloff);
    }
  }

  return result;
}

// Flatten terrain to the average height within the brush radius.
inline auto flatten_terrain(heightmap& height_map, const grid& terrain_grid, std::float_t world_x, std::float_t world_z, std::float_t radius, std::float_t blend_strength = 1.0f, std::int32_t guard_radius = 1) -> sculpt_result {
  auto result = _compute_brush_bounds(height_map, world_x, world_z, radius);

  // First pass: compute weighted average height.
  auto total_height = 0.0f;
  auto total_weight = 0.0f;

  for (auto vz = result.min_vertex_z; vz <= result.max_vertex_z; ++vz) {
    for (auto vx = result.min_vertex_x; vx <= result.max_vertex_x; ++vx) {
      auto falloff = _brush_falloff(vx, vz, world_x, world_z, radius, height_map);

      if (falloff < 0.0f) {
        continue;
      }

      total_height += height_map.get_height(vx, vz) * falloff;
      total_weight += falloff;
    }
  }

  if (total_weight < 0.001f) {
    return result;
  }

  auto target_height = total_height / total_weight;

  // Second pass: blend toward target.
  for (auto vz = result.min_vertex_z; vz <= result.max_vertex_z; ++vz) {
    for (auto vx = result.min_vertex_x; vx <= result.max_vertex_x; ++vx) {
      if (_is_vertex_protected(terrain_grid, vx, vz, guard_radius)) {
        continue;
      }

      auto falloff = _brush_falloff(vx, vz, world_x, world_z, radius, height_map);

      if (falloff < 0.0f) {
        continue;
      }

      auto current = height_map.get_height(vx, vz);
      auto blend = falloff * std::clamp(blend_strength, 0.0f, 1.0f);
      height_map.set_height(vx, vz, std::lerp(current, target_height, blend));
    }
  }

  return result;
}

// Smooth terrain by averaging each vertex with its neighbors.
inline auto smooth_terrain(heightmap& height_map, const grid& terrain_grid, std::float_t world_x, std::float_t world_z, std::float_t radius, std::float_t blend_strength = 0.5f, std::int32_t guard_radius = 1) -> sculpt_result {
  auto result = _compute_brush_bounds(height_map, world_x, world_z, radius);

  // Collect current heights first to avoid read-after-write within the pass.
  auto width = result.max_vertex_x - result.min_vertex_x + 1;
  auto depth = result.max_vertex_z - result.min_vertex_z + 1;

  auto snapshot = std::vector<std::float_t>{};
  snapshot.resize(static_cast<std::size_t>(width * depth));

  for (auto vz = result.min_vertex_z; vz <= result.max_vertex_z; ++vz) {
    for (auto vx = result.min_vertex_x; vx <= result.max_vertex_x; ++vx) {
      auto local_x = vx - result.min_vertex_x;
      auto local_z = vz - result.min_vertex_z;
      snapshot[static_cast<std::size_t>(local_z * width + local_x)] = height_map.get_height(vx, vz);
    }
  }

  auto sample = [&](std::int32_t vx, std::int32_t vz) -> std::float_t {
    auto local_x = vx - result.min_vertex_x;
    auto local_z = vz - result.min_vertex_z;

    if (local_x >= 0 && local_x < width && local_z >= 0 && local_z < depth) {
      return snapshot[static_cast<std::size_t>(local_z * width + local_x)];
    }

    return height_map.get_height(vx, vz);
  };

  for (auto vz = result.min_vertex_z; vz <= result.max_vertex_z; ++vz) {
    for (auto vx = result.min_vertex_x; vx <= result.max_vertex_x; ++vx) {
      if (_is_vertex_protected(terrain_grid, vx, vz, guard_radius)) {
        continue;
      }

      auto falloff = _brush_falloff(vx, vz, world_x, world_z, radius, height_map);

      if (falloff < 0.0f) {
        continue;
      }

      auto center = sample(vx, vz);
      auto neighbor_sum = sample(vx - 1, vz) + sample(vx + 1, vz) + sample(vx, vz - 1) + sample(vx, vz + 1);
      auto smoothed = (center + neighbor_sum) / 5.0f;

      auto blend = falloff * std::clamp(blend_strength, 0.0f, 1.0f);
      height_map.set_height(vx, vz, std::lerp(center, smoothed, blend));
    }
  }

  return result;
}

// Level terrain: set all vertices in radius to the height at the brush center.
inline auto level_terrain(heightmap& height_map, const grid& terrain_grid, std::float_t world_x, std::float_t world_z, std::float_t radius, std::float_t blend_strength = 1.0f, std::int32_t guard_radius = 1) -> sculpt_result {
  auto result = _compute_brush_bounds(height_map, world_x, world_z, radius);

  auto target_height = height_map.get_height_at(world_x, world_z);

  for (auto vz = result.min_vertex_z; vz <= result.max_vertex_z; ++vz) {
    for (auto vx = result.min_vertex_x; vx <= result.max_vertex_x; ++vx) {
      if (_is_vertex_protected(terrain_grid, vx, vz, guard_radius)) {
        continue;
      }

      auto falloff = _brush_falloff(vx, vz, world_x, world_z, radius, height_map);

      if (falloff < 0.0f) {
        continue;
      }

      auto current = height_map.get_height(vx, vz);
      auto blend = falloff * std::clamp(blend_strength, 0.0f, 1.0f);
      height_map.set_height(vx, vz, std::lerp(current, target_height, blend));
    }
  }

  return result;
}

// Legacy: flatten for building placement (no falloff, no protection — used internally).
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
