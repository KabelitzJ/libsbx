// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_SPLAT_GENERATOR_HPP_
#define DEMO_TERRAIN_SPLAT_GENERATOR_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <libsbx/utility/logger.hpp>

#include <demo/terrain/heightmap.hpp>
#include <demo/terrain/province_map.hpp>
#include <demo/terrain/terrain_types.hpp>

namespace demo {

inline auto base_splat_for(landform land) -> splat_weights {
  switch (land) {
    case landform::lake:
      return splat_weights::from_floats(0.00f, 0.10f, 0.00f, 0.55f, 0.00f, 0.35f);
    case landform::plains:
      return splat_weights::from_floats(0.85f, 0.12f, 0.00f, 0.00f, 0.00f, 0.03f);
    case landform::hills:
      return splat_weights::from_floats(0.55f, 0.30f, 0.12f, 0.00f, 0.00f, 0.03f);
    case landform::mountains:
      // mountains: bottom/middle is rock, snow is added height-driven elsewhere
      return splat_weights::from_floats(0.00f, 0.10f, 0.90f, 0.00f, 0.00f, 0.00f);
  }

  return splat_weights::from_floats(1.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f);
}

// Splat to show at "edge height" — what this tier looks like when sitting flat
// at the height of the lower tier (the inset region near borders).
inline auto edge_splat_for(landform land) -> splat_weights {
  switch (land) {
    case landform::lake:      return base_splat_for(landform::plains);
    case landform::plains:    return base_splat_for(landform::plains);
    case landform::hills:     return base_splat_for(landform::plains);
    case landform::mountains: return base_splat_for(landform::hills);
  }

  return base_splat_for(landform::plains);
}

// Heightmap reference points — must match Python EDGE_TARGET / TERRAIN_INTERIOR
// values, so the splat blend tracks the same ramp the heightmap uses.
struct tier_heights {
  std::float_t edge;
  std::float_t interior;
}; // struct tier_heights

inline auto tier_heights_for(landform land) -> tier_heights {
  switch (land) {
    case landform::lake:      return {0.13f, 0.03f};
    case landform::plains:    return {0.13f, 0.13f};
    case landform::hills:     return {0.13f, 0.17f};
    case landform::mountains: return {0.17f, 0.85f};
  }

  return {0.0f, 1.0f};
}

struct splat_generation_params {
  std::float_t slope_threshold_low{0.5f};
  std::float_t slope_threshold_high{2.0f};
  std::float_t rock_boost_max{0.7f};
  // snow blends in based on normalized [0..1] height; independent of height_scale
  std::float_t snow_normalized_start{0.45f};
  std::float_t snow_normalized_full{0.70f};
}; // struct splat_generation_params

inline auto generate_splat_from_provinces(const province_map& provinces, const heightmap& heights, const splat_generation_params& params = {}) -> std::vector<splat_weights> {
  auto cells_x = heights.vertices_x() - 1u;
  auto cells_z = heights.vertices_z() - 1u;

  auto cell_size = heights.cell_size();
  auto inverse_height_scale = 1.0f / std::max(heights.height_scale(), 0.001f);

  auto result = std::vector<splat_weights>{};
  result.resize(static_cast<std::size_t>(cells_x) * static_cast<std::size_t>(cells_z));

  auto slope_range_inverse = 1.0f / (params.slope_threshold_high - params.slope_threshold_low);
  auto snow_range_inverse = 1.0f / std::max(params.snow_normalized_full - params.snow_normalized_start, 0.001f);

  auto max_height_seen = std::float_t{0.0f};
  auto mountain_cells = std::size_t{0};
  auto snowy_mountain_cells = std::size_t{0};

  for (auto cz = std::uint32_t{0}; cz < cells_z; ++cz) {
    for (auto cx = std::uint32_t{0}; cx < cells_x; ++cx) {
      auto pixel_x = std::min(cx, provinces.width() - 1u);
      auto pixel_y = std::min(cz, provinces.height() - 1u);

      auto land = provinces.landform_at_pixel(pixel_x, pixel_y);
      auto base = base_splat_for(land);
      auto edge = edge_splat_for(land);
      auto tier = tier_heights_for(land);

      auto h00 = heights.get_height(static_cast<std::int32_t>(cx),      static_cast<std::int32_t>(cz));
      auto h10 = heights.get_height(static_cast<std::int32_t>(cx) + 1, static_cast<std::int32_t>(cz));
      auto h01 = heights.get_height(static_cast<std::int32_t>(cx),      static_cast<std::int32_t>(cz) + 1);

      auto dh_dx = (h10 - h00) / cell_size;
      auto dh_dz = (h01 - h00) / cell_size;
      auto slope = std::sqrt(dh_dx * dh_dx + dh_dz * dh_dz);

      // Tier-shape t: 0 at edge height (flat inset), 1 at interior height (deep in tier).
      // Only applied to rising tiers (hills, mountains) where we want the inset region
      // near borders to look like the lower tier. Lakes and plains use the base splat
      // directly — for lakes, depression depth varies but the splat shouldn't.
      auto tier_t = std::float_t{1.0f};

      if (land == landform::hills || land == landform::mountains) {
        auto edge_world = tier.edge * heights.height_scale();
        auto interior_world = tier.interior * heights.height_scale();
        auto tier_range = interior_world - edge_world;

        if (std::abs(tier_range) > 0.001f) {
          auto signed_t = (h00 - edge_world) / tier_range;
          tier_t = std::clamp(signed_t, 0.0f, 1.0f);
        }
      }

      // Blend edge_splat → base_splat by tier_t
      auto blend = [tier_t](std::uint8_t a, std::uint8_t b) -> std::float_t {
        auto af = static_cast<std::float_t>(a) / 255.0f;
        auto bf = static_cast<std::float_t>(b) / 255.0f;
        return af + (bf - af) * tier_t;
      };

      auto base_grass = blend(edge.grass, base.grass);
      auto base_dirt  = blend(edge.dirt,  base.dirt);
      auto base_rock  = blend(edge.rock,  base.rock);
      auto base_sand  = blend(edge.sand,  base.sand);
      auto base_snow  = blend(edge.snow,  base.snow);
      auto base_mud   = blend(edge.mud,   base.mud);

      auto rock_boost = std::clamp((slope - params.slope_threshold_low) * slope_range_inverse, 0.0f, 1.0f);

      auto float_grass = base_grass * (1.0f - rock_boost);
      auto float_dirt  = base_dirt  * (1.0f - rock_boost * 0.5f);
      auto float_rock  = base_rock  + rock_boost * params.rock_boost_max;
      auto float_sand  = base_sand;
      auto float_snow  = base_snow;
      auto float_mud   = base_mud;

      // height-driven snow — only on mountains, blends rock → snow with elevation
      if (land == landform::mountains) {
        auto h_normalized = h00 * inverse_height_scale;
        auto snow_t = std::clamp((h_normalized - params.snow_normalized_start) * snow_range_inverse, 0.0f, 1.0f);
        float_snow = snow_t;
        float_rock = float_rock * (1.0f - snow_t);
        float_dirt = float_dirt * (1.0f - snow_t);

        max_height_seen = std::max(max_height_seen, h00);
        ++mountain_cells;
        if (snow_t > 0.01f) {
          ++snowy_mountain_cells;
        }
      }

      result[static_cast<std::size_t>(cz) * cells_x + cx] = splat_weights::from_floats(float_grass, float_dirt, float_rock, float_sand, float_snow, float_mud);
    }
  }

  sbx::utility::logger<"terrain">::info("Splat: max mountain height seen = {:.2f}m (norm: {:.3f}); mountain cells: {}, with snow: {}", max_height_seen, max_height_seen * inverse_height_scale, mountain_cells, snowy_mountain_cells);

  return result;
}

} // namespace demo

#endif // DEMO_TERRAIN_SPLAT_GENERATOR_HPP_
