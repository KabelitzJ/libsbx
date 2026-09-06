// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_TERRAIN_HEIGHTMAP_GENERATOR_HPP_
#define LIBSBX_TERRAIN_HEIGHTMAP_GENERATOR_HPP_

#include <cstdint>
#include <vector>

#include <libsbx/math/noise.hpp>

#include <libsbx/terrain/heightmap.hpp>

namespace sbx::terrain {

struct heightmap_generator_settings {
  std::uint32_t width{129u};
  std::uint32_t depth{129u};
  std::float_t cell_size{1.0f};
  std::float_t frequency{0.02f};  // world-space noise frequency; lower = broader features
  std::float_t amplitude{10.0f};  // vertical scale of the generated terrain
  std::uint32_t octaves{4u};
}; // struct heightmap_generator_settings

/**
 * @brief Builds a default heightmap by sampling math::noise::fractal over the grid -- a
 * placeholder generator until a real map-authoring/import workflow exists. A flat map is just
 * amplitude == 0, not a separate code path.
 */
[[nodiscard]] inline auto generate_heightmap(const heightmap_generator_settings& settings) -> heightmap {
  auto heights = std::vector<std::float_t>(static_cast<std::size_t>(settings.width) * settings.depth, 0.0f);

  if (settings.width == 0u || settings.depth == 0u) {
    return heightmap{settings.width, settings.depth, settings.cell_size, std::move(heights)};
  }

  const auto half_extent_x = static_cast<std::float_t>(settings.width - 1u) * settings.cell_size * 0.5f;
  const auto half_extent_z = static_cast<std::float_t>(settings.depth - 1u) * settings.cell_size * 0.5f;

  for (auto z = std::uint32_t{0}; z < settings.depth; ++z) {
    for (auto x = std::uint32_t{0}; x < settings.width; ++x) {
      const auto world_x = static_cast<std::float_t>(x) * settings.cell_size - half_extent_x;
      const auto world_z = static_cast<std::float_t>(z) * settings.cell_size - half_extent_z;

      const auto noise = math::noise::fractal(world_x * settings.frequency, world_z * settings.frequency, settings.octaves);

      heights[static_cast<std::size_t>(z) * settings.width + x] = noise * settings.amplitude;
    }
  }

  return heightmap{settings.width, settings.depth, settings.cell_size, std::move(heights)};
}

} // namespace sbx::terrain

#endif // LIBSBX_TERRAIN_HEIGHTMAP_GENERATOR_HPP_
