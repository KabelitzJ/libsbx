// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_CHUNK_HPP_
#define DEMO_TERRAIN_CHUNK_HPP_

#include <array>
#include <vector>
#include <cstdint>
#include <functional>
#include <algorithm>

#include <libsbx/utility/hash.hpp>

namespace demo {

static constexpr auto chunk_size = 32u;

struct cell_coordinates {

  std::int32_t x{};
  std::int32_t z{};

  auto operator==(const cell_coordinates&) const -> bool = default;

};

struct cell_coordinates_hash {

  auto operator()(const cell_coordinates& coordinates) const -> std::size_t {
    auto seed = std::size_t{0};

    sbx::utility::hash_combine(seed, coordinates.x, coordinates.z);

    return seed;
  }

};

struct world_coordinates {

  std::float_t x{};
  std::float_t z{};

  auto operator==(const world_coordinates&) const -> bool = default;

};

struct world_coordinates_hash {

  auto operator()(const world_coordinates& coordinates) const -> std::size_t {
    auto seed = std::size_t{0};

    sbx::utility::hash_combine(seed, coordinates.x, coordinates.z);

    return seed;
  }

};

struct cell {
  std::uint16_t district_id{0};
  std::uint16_t building_id{0};
  std::uint8_t terrain_type{0};
  std::uint8_t zone_type{0};
  std::uint8_t flags{0};
  std::uint8_t road_type{0};
  std::uint8_t road_mask{0};
};

struct grid_chunk {

  std::array<cell, chunk_size * chunk_size> cells{};
  bool is_dirty{true};

  auto at(const cell_coordinates& coordinates) -> cell& {
    return cells[static_cast<std::uint32_t>(coordinates.z) * chunk_size + static_cast<std::uint32_t>(coordinates.x)];
  }

  auto at(const cell_coordinates& coordinates) const -> const cell& {
    return cells[static_cast<std::uint32_t>(coordinates.z) * chunk_size + static_cast<std::uint32_t>(coordinates.x)];
  }

};

struct splat_weights {
  std::uint8_t grass{255};
  std::uint8_t dirt{0};
  std::uint8_t rock{0};
  std::uint8_t sand{0};
  std::uint8_t snow{0};
  std::uint8_t mud{0};
  std::uint8_t _padding0{0};
  std::uint8_t _padding1{0};

  static auto from_floats(std::float_t grass, std::float_t dirt, std::float_t rock, std::float_t sand, std::float_t snow, std::float_t mud) -> splat_weights {
    auto total = grass + dirt + rock + sand + snow + mud;

    if (total > 0.0f) {
      auto inverse = 1.0f / total;

      grass *= inverse;
      dirt *= inverse;
      rock *= inverse;
      sand *= inverse;
      snow *= inverse;
      mud *= inverse;
    }

    return splat_weights{
      .grass = static_cast<std::uint8_t>(std::clamp(grass, 0.0f, 1.0f) * 255.0f),
      .dirt = static_cast<std::uint8_t>(std::clamp(dirt, 0.0f, 1.0f) * 255.0f),
      .rock = static_cast<std::uint8_t>(std::clamp(rock, 0.0f, 1.0f) * 255.0f),
      .sand = static_cast<std::uint8_t>(std::clamp(sand, 0.0f, 1.0f) * 255.0f),
      .snow = static_cast<std::uint8_t>(std::clamp(snow, 0.0f, 1.0f) * 255.0f),
      .mud = static_cast<std::uint8_t>(std::clamp(mud, 0.0f, 1.0f) * 255.0f),
    };
  }

};

static_assert(sizeof(splat_weights) == 8, "splat_weights must be 8 bytes for GPU alignment");

} // namespace demo

#endif // DEMO_TERRAIN_CHUNK_HPP_
