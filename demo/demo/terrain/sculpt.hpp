// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_SCULPT_HPP_
#define DEMO_TERRAIN_SCULPT_HPP_

#include <cstdint>
#include <cmath>
#include <algorithm>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/heightmap.hpp>
#include <demo/terrain/grid.hpp>

namespace demo {

struct sculpt_result {
  chunk_coord min_chunk;
  chunk_coord max_chunk;
  std::int32_t min_vx{};
  std::int32_t min_vy{};
  std::int32_t max_vx{};
  std::int32_t max_vy{};
}; // struct sculpt_result

inline auto sculpt_terrain(heightmap& heightmap, std::float_t world_x, std::float_t world_z, std::float_t radius, std::float_t strength) -> sculpt_result {
  auto cs = grid::cell_size;
  auto offset_x = static_cast<std::float_t>(heightmap.world_width()) * cs * 0.5f;
  auto offset_z = static_cast<std::float_t>(heightmap.world_height()) * cs * 0.5f;

  auto grid_x = world_x + offset_x;
  auto grid_z = world_z + offset_z;

  auto cx = static_cast<std::int32_t>(grid_x / cs);
  auto cz = static_cast<std::int32_t>(grid_z / cs);
  auto r = static_cast<std::int32_t>(std::ceil(radius / cs));

  auto result = sculpt_result{};
  result.min_vx = std::max(cx - r, 0);
  result.min_vy = std::max(cz - r, 0);
  result.max_vx = std::min(cx + r, static_cast<std::int32_t>(heightmap.verts_w() - 1));
  result.max_vy = std::min(cz + r, static_cast<std::int32_t>(heightmap.verts_h() - 1));

  for (auto vy = result.min_vy; vy <= result.max_vy; ++vy) {
    for (auto vx = result.min_vx; vx <= result.max_vx; ++vx) {
      auto dx = static_cast<std::float_t>(vx) * cs - grid_x;
      auto dz = static_cast<std::float_t>(vy) * cs - grid_z;
      auto dist = std::sqrt(dx * dx + dz * dz);

      if (dist < radius) {
        auto falloff = 1.0f - (dist / radius);
        falloff *= falloff;

        auto current = heightmap.get_height(vx, vy);
        heightmap.set_height(vx, vy, current + strength * falloff);
      }
    }
  }

  result.min_chunk = chunk_coord{
    static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(result.min_vx) / chunk_size)),
    static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(result.min_vy) / chunk_size))
  };

  result.max_chunk = chunk_coord{
    static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(result.max_vx) / chunk_size)),
    static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(result.max_vy) / chunk_size))
  };

  return result;
}

inline auto flatten_for_building(heightmap& heightmap, std::int32_t cell_x, std::int32_t cell_y, std::uint32_t size_w, std::uint32_t size_h) -> void {
  auto avg = 0.0f;
  auto count = 0u;

  for (auto vy = cell_y; vy <= cell_y + static_cast<std::int32_t>(size_h); ++vy) {
    for (auto vx = cell_x; vx <= cell_x + static_cast<std::int32_t>(size_w); ++vx) {
      avg += heightmap.get_height(vx, vy);
      ++count;
    }
  }

  avg /= static_cast<std::float_t>(count);

  for (auto vy = cell_y; vy <= cell_y + static_cast<std::int32_t>(size_h); ++vy) {
    for (auto vx = cell_x; vx <= cell_x + static_cast<std::int32_t>(size_w); ++vx) {
      heightmap.set_height(vx, vy, avg);
    }
  }
}

inline auto get_slope_cost(const heightmap& heightmap, std::int32_t cell_x, std::int32_t cell_y) -> std::float_t {
  auto h00 = heightmap.get_height(cell_x, cell_y);
  auto h10 = heightmap.get_height(cell_x + 1, cell_y);
  auto h01 = heightmap.get_height(cell_x, cell_y + 1);
  auto h11 = heightmap.get_height(cell_x + 1, cell_y + 1);

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