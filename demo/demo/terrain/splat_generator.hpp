// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_SPLAT_GENERATOR_HPP_
#define DEMO_TERRAIN_SPLAT_GENERATOR_HPP_

#include <cstdint>
#include <cmath>
#include <algorithm>

#include <libsbx/math/noise.hpp>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/heightmap.hpp>

namespace demo {

struct splat_generation_params {
  std::float_t variety_scale{0.05f};
  std::float_t variety_strength{0.15f};
  std::float_t moisture_scale{0.02f};
  std::float_t seed{0.0f};
}; // struct splat_generation_params

inline auto generate_splat_for_chunk(splat_chunk& splat, const heightmap& height_map, chunk_coord chunk_coordinates, const splat_generation_params& params = {}) -> void {
  auto base_x = chunk_coordinates.x * static_cast<std::int32_t>(chunk_size);
  auto base_y = chunk_coordinates.y * static_cast<std::int32_t>(chunk_size);

  for (auto local_y = 0u; local_y < chunk_size; ++local_y) {
    for (auto local_x = 0u; local_x < chunk_size; ++local_x) {
      auto global_x = base_x + static_cast<std::int32_t>(local_x);
      auto global_y = base_y + static_cast<std::int32_t>(local_y);

      auto h00 = height_map.get_height(global_x, global_y);
      auto h10 = height_map.get_height(global_x + 1, global_y);
      auto h01 = height_map.get_height(global_x, global_y + 1);
      auto h11 = height_map.get_height(global_x + 1, global_y + 1);

      auto average_height = (h00 + h10 + h01 + h11) * 0.25f;

      auto slope = std::max({
        std::abs(h00 - h10),
        std::abs(h00 - h01),
        std::abs(h10 - h11),
        std::abs(h01 - h11)
      });

      auto world_x = static_cast<std::float_t>(global_x);
      auto world_z = static_cast<std::float_t>(global_y);

      auto variety_noise = sbx::math::noise::simplex(world_x * params.variety_scale + params.seed, world_z * params.variety_scale + params.seed) * params.variety_strength;

      auto moisture_noise = sbx::math::noise::fractal(world_x * params.moisture_scale + params.seed + 500.0f, world_z * params.moisture_scale + params.seed + 500.0f, 2u);

      // Height relative to sea level
      auto depth_below_sea = terrain_constants::sea_level - average_height;
      auto height_above_sea = average_height - terrain_constants::sea_level;

      auto grass = 0.0f;
      auto dirt = 0.0f;
      auto rock = 0.0f;
      auto sand = 0.0f;
      auto snow = 0.0f;
      auto mud = 0.0f;

      if (average_height < terrain_constants::sea_level) {
        // Below water — river/lake bed
        // Deep = sand, shallow = mud
        auto depth_factor = std::clamp(depth_below_sea / (terrain_constants::sea_level - terrain_constants::lake_bed_min), 0.0f, 1.0f);
        sand = depth_factor;
        mud = 1.0f - depth_factor;
      } else if (average_height < terrain_constants::shore_max) {
        // Shore band — transition from mud/sand to grass
        auto shore_factor = std::clamp((average_height - terrain_constants::sea_level) / (terrain_constants::shore_max - terrain_constants::sea_level), 0.0f, 1.0f);
        auto moisture_factor = std::clamp(moisture_noise * 0.5f + 0.5f, 0.0f, 1.0f);
        mud = (1.0f - shore_factor) * moisture_factor;
        sand = (1.0f - shore_factor) * (1.0f - moisture_factor);
        grass = shore_factor;
      } else if (average_height < terrain_constants::plains_max) {
        // Plains — mostly grass with noise variation
        grass = 1.0f + variety_noise;
        dirt = -variety_noise * 0.5f;
      } else if (average_height < terrain_constants::hills_max) {
        // Hills — grass/dirt blend
        auto hill_factor = std::clamp((average_height - terrain_constants::plains_max) / (terrain_constants::hills_max - terrain_constants::plains_max), 0.0f, 1.0f);
        grass = 1.0f - hill_factor * 0.6f;
        dirt = hill_factor * 0.6f;
      } else {
        // Mountains — dirt/rock/snow
        auto mountain_factor = std::clamp((average_height - terrain_constants::hills_max) / (terrain_constants::mountain_max - terrain_constants::hills_max), 0.0f, 1.0f);
        dirt = (1.0f - mountain_factor) * 0.4f;
        rock = 0.6f + mountain_factor * 0.2f;
        snow = mountain_factor * 0.8f;
      }

      // Steep slopes become rock regardless of height band
      if (slope > 0.5f) {
        auto slope_factor = std::clamp((slope - 0.5f) / 2.0f, 0.0f, 1.0f);
        auto rock_addition = slope_factor * 0.8f;

        grass *= (1.0f - rock_addition);
        dirt *= (1.0f - rock_addition);
        sand *= (1.0f - rock_addition);
        mud *= (1.0f - rock_addition);
        snow *= (1.0f - slope_factor * 0.5f); // snow slides off steep slopes
        rock += rock_addition;
      }

      // Clamp
      grass = std::max(grass, 0.0f);
      dirt = std::max(dirt, 0.0f);
      rock = std::max(rock, 0.0f);
      sand = std::max(sand, 0.0f);
      snow = std::max(snow, 0.0f);
      mud = std::max(mud, 0.0f);

      splat.at(local_x, local_y) = splat_weights::from_floats(grass, dirt, rock, sand, snow, mud);
    }
  }

  splat.is_dirty = true;
}

} // namespace demo

#endif // DEMO_TERRAIN_SPLAT_GENERATOR_HPP_
