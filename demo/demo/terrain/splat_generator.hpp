// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_SPLAT_GENERATOR_HPP_
#define DEMO_TERRAIN_SPLAT_GENERATOR_HPP_

#include <vector>
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

inline auto generate_splat_map(const heightmap& height_map, const splat_generation_params& params = {}) -> std::vector<splat_weights> {
  auto cell_count_x = height_map.world_width();
  auto cell_count_z = height_map.world_height();

  auto weights = std::vector<splat_weights>{};
  weights.resize(cell_count_x * cell_count_z);

  for (auto cell_z = 0u; cell_z < cell_count_z; ++cell_z) {
    for (auto cell_x = 0u; cell_x < cell_count_x; ++cell_x) {
      auto global_x = static_cast<std::int32_t>(cell_x);
      auto global_z = static_cast<std::int32_t>(cell_z);

      auto h00 = height_map.get_height(global_x, global_z);
      auto h10 = height_map.get_height(global_x + 1, global_z);
      auto h01 = height_map.get_height(global_x, global_z + 1);
      auto h11 = height_map.get_height(global_x + 1, global_z + 1);

      auto average_height = (h00 + h10 + h01 + h11) * 0.25f;

      auto slope = std::max({
        std::abs(h00 - h10),
        std::abs(h00 - h01),
        std::abs(h10 - h11),
        std::abs(h01 - h11)
      });

      auto world_x = static_cast<std::float_t>(global_x);
      auto world_z = static_cast<std::float_t>(global_z);

      auto variety_noise = sbx::math::noise::simplex(world_x * params.variety_scale + params.seed, world_z * params.variety_scale + params.seed) * params.variety_strength;

      auto moisture_noise = sbx::math::noise::fractal(world_x * params.moisture_scale + params.seed + 500.0f, world_z * params.moisture_scale + params.seed + 500.0f, 2u);

      auto depth_below_sea = terrain_constants::sea_level - average_height;

      auto grass = 0.0f;
      auto dirt = 0.0f;
      auto rock = 0.0f;
      auto sand = 0.0f;
      auto snow = 0.0f;
      auto mud = 0.0f;

      if (average_height < terrain_constants::sea_level) {
        auto depth_factor = std::clamp(depth_below_sea / (terrain_constants::sea_level - terrain_constants::lake_bed_min), 0.0f, 1.0f);
        sand = depth_factor;
        mud = 1.0f - depth_factor;
      } else if (average_height < terrain_constants::shore_max) {
        auto shore_factor = std::clamp((average_height - terrain_constants::sea_level) / (terrain_constants::shore_max - terrain_constants::sea_level), 0.0f, 1.0f);
        auto moisture_factor = std::clamp(moisture_noise * 0.5f + 0.5f, 0.0f, 1.0f);
        mud = (1.0f - shore_factor) * moisture_factor;
        sand = (1.0f - shore_factor) * (1.0f - moisture_factor);
        grass = shore_factor;
      } else if (average_height < terrain_constants::plains_max) {
        grass = 1.0f + variety_noise;
        dirt = -variety_noise * 0.5f;
      } else if (average_height < terrain_constants::hills_max) {
        auto hill_factor = std::clamp((average_height - terrain_constants::plains_max) / (terrain_constants::hills_max - terrain_constants::plains_max), 0.0f, 1.0f);
        grass = 1.0f - hill_factor * 0.6f;
        dirt = hill_factor * 0.6f;
      } else {
        auto mountain_factor = std::clamp((average_height - terrain_constants::hills_max) / (terrain_constants::mountain_max - terrain_constants::hills_max), 0.0f, 1.0f);
        dirt = (1.0f - mountain_factor) * 0.4f;
        rock = 0.6f + mountain_factor * 0.2f;
        snow = mountain_factor * 0.8f;
      }

      if (slope > 0.5f) {
        auto slope_factor = std::clamp((slope - 0.5f) / 2.0f, 0.0f, 1.0f);
        auto rock_addition = slope_factor * 0.8f;

        grass *= (1.0f - rock_addition);
        dirt *= (1.0f - rock_addition);
        sand *= (1.0f - rock_addition);
        mud *= (1.0f - rock_addition);
        snow *= (1.0f - slope_factor * 0.5f);
        rock += rock_addition;
      }

      grass = std::max(grass, 0.0f);
      dirt = std::max(dirt, 0.0f);
      rock = std::max(rock, 0.0f);
      sand = std::max(sand, 0.0f);
      snow = std::max(snow, 0.0f);
      mud = std::max(mud, 0.0f);

      weights[cell_z * cell_count_x + cell_x] = splat_weights::from_floats(grass, dirt, rock, sand, snow, mud);
    }
  }

  return weights;
}

inline auto regenerate_splat_region(std::vector<splat_weights>& weights, const heightmap& height_map, std::int32_t min_x, std::int32_t min_z, std::int32_t max_x, std::int32_t max_z, const splat_generation_params& params = {}) -> void {
  auto cell_count_x = height_map.world_width();
  auto cell_count_z = height_map.world_height();

  min_x = std::clamp(min_x, 0, static_cast<std::int32_t>(cell_count_x - 1));
  min_z = std::clamp(min_z, 0, static_cast<std::int32_t>(cell_count_z - 1));
  max_x = std::clamp(max_x, 0, static_cast<std::int32_t>(cell_count_x - 1));
  max_z = std::clamp(max_z, 0, static_cast<std::int32_t>(cell_count_z - 1));

  for (auto cell_z = min_z; cell_z <= max_z; ++cell_z) {
    for (auto cell_x = min_x; cell_x <= max_x; ++cell_x) {
      auto h00 = height_map.get_height(cell_x, cell_z);
      auto h10 = height_map.get_height(cell_x + 1, cell_z);
      auto h01 = height_map.get_height(cell_x, cell_z + 1);
      auto h11 = height_map.get_height(cell_x + 1, cell_z + 1);

      auto average_height = (h00 + h10 + h01 + h11) * 0.25f;

      auto slope = std::max({
        std::abs(h00 - h10),
        std::abs(h00 - h01),
        std::abs(h10 - h11),
        std::abs(h01 - h11)
      });

      auto world_x = static_cast<std::float_t>(cell_x);
      auto world_z = static_cast<std::float_t>(cell_z);

      auto variety_noise = sbx::math::noise::simplex(world_x * params.variety_scale + params.seed, world_z * params.variety_scale + params.seed) * params.variety_strength;

      auto moisture_noise = sbx::math::noise::fractal(world_x * params.moisture_scale + params.seed + 500.0f, world_z * params.moisture_scale + params.seed + 500.0f, 2u);

      auto depth_below_sea = terrain_constants::sea_level - average_height;

      auto grass = 0.0f;
      auto dirt = 0.0f;
      auto rock = 0.0f;
      auto sand = 0.0f;
      auto snow = 0.0f;
      auto mud = 0.0f;

      if (average_height < terrain_constants::sea_level) {
        auto depth_factor = std::clamp(depth_below_sea / (terrain_constants::sea_level - terrain_constants::lake_bed_min), 0.0f, 1.0f);
        sand = depth_factor;
        mud = 1.0f - depth_factor;
      } else if (average_height < terrain_constants::shore_max) {
        auto shore_factor = std::clamp((average_height - terrain_constants::sea_level) / (terrain_constants::shore_max - terrain_constants::sea_level), 0.0f, 1.0f);
        auto moisture_factor = std::clamp(moisture_noise * 0.5f + 0.5f, 0.0f, 1.0f);
        mud = (1.0f - shore_factor) * moisture_factor;
        sand = (1.0f - shore_factor) * (1.0f - moisture_factor);
        grass = shore_factor;
      } else if (average_height < terrain_constants::plains_max) {
        grass = 1.0f + variety_noise;
        dirt = -variety_noise * 0.5f;
      } else if (average_height < terrain_constants::hills_max) {
        auto hill_factor = std::clamp((average_height - terrain_constants::plains_max) / (terrain_constants::hills_max - terrain_constants::plains_max), 0.0f, 1.0f);
        grass = 1.0f - hill_factor * 0.6f;
        dirt = hill_factor * 0.6f;
      } else {
        auto mountain_factor = std::clamp((average_height - terrain_constants::hills_max) / (terrain_constants::mountain_max - terrain_constants::hills_max), 0.0f, 1.0f);
        dirt = (1.0f - mountain_factor) * 0.4f;
        rock = 0.6f + mountain_factor * 0.2f;
        snow = mountain_factor * 0.8f;
      }

      if (slope > 0.5f) {
        auto slope_factor = std::clamp((slope - 0.5f) / 2.0f, 0.0f, 1.0f);
        auto rock_addition = slope_factor * 0.8f;

        grass *= (1.0f - rock_addition);
        dirt *= (1.0f - rock_addition);
        sand *= (1.0f - rock_addition);
        mud *= (1.0f - rock_addition);
        snow *= (1.0f - slope_factor * 0.5f);
        rock += rock_addition;
      }

      grass = std::max(grass, 0.0f);
      dirt = std::max(dirt, 0.0f);
      rock = std::max(rock, 0.0f);
      sand = std::max(sand, 0.0f);
      snow = std::max(snow, 0.0f);
      mud = std::max(mud, 0.0f);

      weights[static_cast<std::uint32_t>(cell_z) * cell_count_x + static_cast<std::uint32_t>(cell_x)] = splat_weights::from_floats(grass, dirt, rock, sand, snow, mud);
    }
  }
}

} // namespace demo

#endif // DEMO_TERRAIN_SPLAT_GENERATOR_HPP_
