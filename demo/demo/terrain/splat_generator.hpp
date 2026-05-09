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

struct splat_generation_params {
  std::float_t slope_threshold_low{0.5f};
  std::float_t slope_threshold_high{2.0f};
  std::float_t rock_boost_max{0.7f};
  // snow blends in based on normalized [0..1] height; independent of height_scale
  std::float_t snow_normalized_start{0.50f};
  std::float_t snow_normalized_full{0.80f};
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

      auto h00 = heights.get_height(static_cast<std::int32_t>(cx),      static_cast<std::int32_t>(cz));
      auto h10 = heights.get_height(static_cast<std::int32_t>(cx) + 1, static_cast<std::int32_t>(cz));
      auto h01 = heights.get_height(static_cast<std::int32_t>(cx),      static_cast<std::int32_t>(cz) + 1);

      auto dh_dx = (h10 - h00) / cell_size;
      auto dh_dz = (h01 - h00) / cell_size;
      auto slope = std::sqrt(dh_dx * dh_dx + dh_dz * dh_dz);

      auto rock_boost = std::clamp((slope - params.slope_threshold_low) * slope_range_inverse, 0.0f, 1.0f);

      auto float_grass = (static_cast<std::float_t>(base.grass) / 255.0f) * (1.0f - rock_boost);
      auto float_dirt  = (static_cast<std::float_t>(base.dirt)  / 255.0f) * (1.0f - rock_boost * 0.5f);
      auto float_rock  = (static_cast<std::float_t>(base.rock)  / 255.0f) + rock_boost * params.rock_boost_max;
      auto float_sand  =  static_cast<std::float_t>(base.sand)  / 255.0f;
      auto float_snow  =  static_cast<std::float_t>(base.snow)  / 255.0f;
      auto float_mud   =  static_cast<std::float_t>(base.mud)   / 255.0f;

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
