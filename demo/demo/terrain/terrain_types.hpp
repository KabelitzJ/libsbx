// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_TERRAIN_TYPES_HPP_
#define DEMO_TERRAIN_TERRAIN_TYPES_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace demo {

enum class landform : std::uint8_t {
  lake = 0,
  plains = 1,
  hills = 2,
  mountains = 3,
}; // enum class landform

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
      .dirt = static_cast<std::uint8_t>(std::clamp(dirt,  0.0f, 1.0f) * 255.0f),
      .rock = static_cast<std::uint8_t>(std::clamp(rock,  0.0f, 1.0f) * 255.0f),
      .sand = static_cast<std::uint8_t>(std::clamp(sand,  0.0f, 1.0f) * 255.0f),
      .snow = static_cast<std::uint8_t>(std::clamp(snow,  0.0f, 1.0f) * 255.0f),
      .mud = static_cast<std::uint8_t>(std::clamp(mud,   0.0f, 1.0f) * 255.0f),
    };
  }

}; // struct splat_weights

static_assert(sizeof(splat_weights) == 8, "splat_weights must be 8 bytes for GPU alignment");

struct world_coordinates {

  std::float_t x{};
  std::float_t z{};

  auto operator==(const world_coordinates&) const -> bool = default;

}; // struct world_coordinates

} // namespace demo

#endif // DEMO_TERRAIN_TERRAIN_TYPES_HPP_
