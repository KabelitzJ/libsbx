// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_CHUNK_HPP_
#define DEMO_TERRAIN_CHUNK_HPP_

#include <array>
#include <vector>
#include <cstdint>
#include <functional>

namespace demo {

static constexpr auto chunk_size  = 32u;
static constexpr auto chunk_vertices = chunk_size + 1u;

struct chunk_coord {

  std::int32_t x{};
  std::int32_t y{};

  auto operator==(const chunk_coord&) const -> bool = default;

}; // struct chunk_coord

struct chunk_coord_hash {

  auto operator()(const chunk_coord& c) const -> std::size_t {
    auto seed = std::size_t{0};

    sbx::utility::hash_combine(seed, c.x, c.y);

    return seed;
  }

}; // struct chunk_coord_hash

struct cell {
  std::uint16_t district_id{0};
  std::uint16_t building_id{0};
  std::uint8_t terrain_type{0};
  std::uint8_t zone_type{0};
  std::uint8_t flags{0};
}; // struct cell

struct grid_chunk {

  std::array<cell, chunk_size * chunk_size> cells{};
  bool is_dirty{true};

  auto at(std::uint32_t local_x, std::uint32_t local_y) -> cell& {
    return cells[local_y * chunk_size + local_x];
  }

  auto at(std::uint32_t local_x, std::uint32_t local_y) const -> const cell& {
    return cells[local_y * chunk_size + local_x];
  }

}; // struct grid_chunk

struct height_chunk {

  std::array<std::float_t, chunk_vertices * chunk_vertices> heights{};
  bool is_dirty{true};

  auto at(std::uint32_t local_x, std::uint32_t local_y) -> std::float_t& {
    return heights[local_y * chunk_vertices + local_x];
  }

  auto at(std::uint32_t local_x, std::uint32_t local_y) const -> std::float_t {
    return heights[local_y * chunk_vertices + local_x];
  }

}; // struct height_chunk

} // namespace demo

#endif // DEMO_TERRAIN_CHUNK_HPP_
