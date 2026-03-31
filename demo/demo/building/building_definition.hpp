// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDINGS_BUILDING_DEFINITION_HPP_
#define DEMO_BUILDINGS_BUILDING_DEFINITION_HPP_

#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>

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

  auto half_w = static_cast<std::float_t>(width) * 0.5f;
  auto half_h = static_cast<std::float_t>(height) * 0.5f;

  // Rotation center in cell-offset space: center of the w×h cell block
  auto center_x = half_w;
  auto center_z = half_h;

  for (auto orient = 0u; orient < orientation_count; ++orient) {
    auto angle_rad = static_cast<std::float_t>(orient) * 45.0f * 3.14159265f / 180.0f;
    auto cos_a = std::cos(angle_rad);
    auto sin_a = std::sin(angle_rad);

    // Rotate the 4 corners of [-half_w, half_w] × [-half_h, half_h]
    // and convert to absolute cell-offset space by adding (center_x, center_z)
    struct vec2 { std::float_t x; std::float_t z; };

    auto rotate = [&](std::float_t x, std::float_t z) -> vec2 {
      return {x * cos_a - z * sin_a + center_x, x * sin_a + z * cos_a + center_z};
    };

    auto c0 = rotate(-half_w, -half_h);
    auto c1 = rotate( half_w, -half_h);
    auto c2 = rotate( half_w,  half_h);
    auto c3 = rotate(-half_w,  half_h);

    // Bounding box in cell-offset space
    auto bb_min_x = std::min({c0.x, c1.x, c2.x, c3.x});
    auto bb_max_x = std::max({c0.x, c1.x, c2.x, c3.x});
    auto bb_min_z = std::min({c0.z, c1.z, c2.z, c3.z});
    auto bb_max_z = std::max({c0.z, c1.z, c2.z, c3.z});

    auto cell_min_x = static_cast<std::int32_t>(std::floor(bb_min_x));
    auto cell_max_x = static_cast<std::int32_t>(std::ceil(bb_max_x));
    auto cell_min_z = static_cast<std::int32_t>(std::floor(bb_min_z));
    auto cell_max_z = static_cast<std::int32_t>(std::ceil(bb_max_z));

    auto& fp = result[orient];

    for (auto cz = cell_min_z; cz < cell_max_z; ++cz) {
      for (auto cx = cell_min_x; cx < cell_max_x; ++cx) {
        // Cell center relative to rotation center
        auto px = static_cast<std::float_t>(cx) + 0.5f - center_x;
        auto pz = static_cast<std::float_t>(cz) + 0.5f - center_z;

        // Inverse-rotate into local rectangle space
        auto local_x = px * cos_a + pz * sin_a;
        auto local_z = px * sin_a + pz * cos_a;

        // Cell center is inside the unrotated rectangle (half-open interval)
        if (local_x >= -half_w && local_x < half_w && local_z >= -half_h && local_z < half_h) {
          fp.push_back({cx, cz});
        }
      }
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
  std::string material_id;

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
