// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDINGS_BUILDING_DEFINITION_HPP_
#define DEMO_BUILDINGS_BUILDING_DEFINITION_HPP_

#include <vector>
#include <array>
#include <string>
#include <cstdint>

#include <demo/terrain/chunk.hpp>

namespace demo {

enum class building_category : std::uint8_t {
  housing,
  extraction,
  processing,
  manufacturing,
  infrastructure,
  civic,
  military
}; // enum class building_category

enum class orientation : std::uint8_t {
  n = 0,
  ne,
  e,
  se,
  s,
  sw,
  w,
  nw
}; // enum class orientation

static constexpr auto orientation_count = 8u;

struct cell_offset {
  std::int32_t x{};
  std::int32_t z{};
}; // struct cell_offset

using footprint = std::vector<cell_offset>;

inline auto compute_footprints(std::uint32_t width, std::uint32_t height) -> std::array<footprint, orientation_count> {
  auto result = std::array<footprint, orientation_count>{};

  auto base_cardinal = std::array<footprint, 4>{};

  for (auto z = 0u; z < height; ++z) {
    for (auto x = 0u; x < width; ++x) {
      base_cardinal[0].push_back({static_cast<std::int32_t>(x), static_cast<std::int32_t>(z)});
    }
  }

  for (auto z = 0u; z < width; ++z) {
    for (auto x = 0u; x < height; ++x) {
      base_cardinal[1].push_back({static_cast<std::int32_t>(x), static_cast<std::int32_t>(z)});
    }
  }

  for (auto z = 0u; z < height; ++z) {
    for (auto x = 0u; x < width; ++x) {
      base_cardinal[2].push_back({-static_cast<std::int32_t>(x), -static_cast<std::int32_t>(z)});
    }
  }

  for (auto z = 0u; z < width; ++z) {
    for (auto x = 0u; x < height; ++x) {
      base_cardinal[3].push_back({-static_cast<std::int32_t>(x), -static_cast<std::int32_t>(z)});
    }
  }

  for (auto i = 0u; i < 4u; ++i) {
    auto cardinal_index = i * 2;
    auto diagonal_index = cardinal_index + 1;

    result[cardinal_index] = base_cardinal[i];

    auto& diagonal = result[diagonal_index];
    diagonal.reserve(base_cardinal[i].size());

    for (const auto& offset : base_cardinal[i]) {
      diagonal.push_back({offset.x + offset.z, offset.z});
    }
  }

  return result;
}

struct building_definition {

  std::uint32_t id{};
  std::string name;
  building_category category{};
  std::uint32_t footprint_width{};
  std::uint32_t footprint_height{};
  std::string mesh_id;

  std::array<footprint, orientation_count> footprints;

  auto precompute_footprints() -> void {
    footprints = compute_footprints(footprint_width, footprint_height);
  }

  auto get_footprint(orientation orient) const -> const footprint& {
    return footprints[static_cast<std::uint8_t>(orient)];
  }

}; // struct building_definition

} // namespace demo

#endif // DEMO_BUILDINGS_BUILDING_DEFINITION_HPP_
