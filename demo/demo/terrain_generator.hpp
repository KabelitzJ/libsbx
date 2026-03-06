// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_GENERATOR_HPP_
#define DEMO_TERRAIN_GENERATOR_HPP_

#include <libsbx/math/noise.hpp>

#include <demo/terrain.hpp>
#include <demo/grid.hpp>

namespace demo {

struct terrain_generation_params {
  float base_scale{0.01f};        // how spread out the terrain is
  float height_scale{15.0f};      // max hill height
  float valley_depth{3.0f};       // how deep valleys cut
  float flatness{0.6f};           // 0 = very hilly, 1 = very flat
  float river_width{0.03f};       // width of river valleys
  float plateau_threshold{0.6f};  // where plateaus start forming
  std::uint32_t octaves{6u};      // noise detail level
  float seed_x{0.0f};             // world offset for variety
  float seed_z{0.0f};
}; // struct terrain_generation_params

auto generate_terrain(terrain& terrain, const terrain_generation_params& params = {}) -> void {
  auto verts_w = terrain.verts_w();
  auto verts_h = terrain.verts_h();

  for (auto vy = 0u; vy < verts_h; ++vy) {
    for (auto vx = 0u; vx < verts_w; ++vx) {
      auto wx = static_cast<float>(vx) + params.seed_x;
      auto wz = static_cast<float>(vy) + params.seed_z;

      // Base terrain shape — large rolling hills
      auto base = sbx::math::noise::fractal(wx * params.base_scale, wz * params.base_scale, params.octaves);

      // Secondary layer — broader features, mountains
      auto mountains = sbx::math::noise::fractal(wx * params.base_scale * 0.5f + 100.0f, wz * params.base_scale * 0.5f + 100.0f, 3u);

      // Detail layer — small bumps and roughness
      auto detail = sbx::math::noise::fractal(wx * params.base_scale * 4.0f + 200.0f, wz * params.base_scale * 4.0f + 200.0f, 2u);

      // River carved valley using a ridge noise trick
      // abs(noise) creates sharp valleys at zero crossings
      auto river_noise = sbx::math::noise::fractal(wx * params.river_width + 300.0f, wz * params.river_width + 300.0f, 3u);

      auto river_carved = 1.0f - std::abs(river_noise);
      river_carved = river_carved * river_carved; // sharpen the valley

      // Plateau effect — flatten the tops of hills
      auto shaped = base;

      if (shaped > params.plateau_threshold) {
        auto excess = shaped - params.plateau_threshold;
        shaped = params.plateau_threshold + excess * 0.2f;
      }

      // Combine layers
      auto height = shaped * params.height_scale + mountains * params.height_scale * 0.4f + detail * 0.8f - (1.0f - river_carved) * params.valley_depth;

      // Apply flatness — lerp toward a base height
      auto flat_height = params.height_scale * 0.2f;
      height = std::lerp(height, flat_height, params.flatness);

      // Gentle falloff at map edges to keep borders buildable
      auto edge_x = static_cast<float>(vx) / static_cast<float>(verts_w - 1);
      auto edge_z = static_cast<float>(vy) / static_cast<float>(verts_h - 1);

      auto edge_fade_x = std::min(edge_x, 1.0f - edge_x) * 4.0f;
      auto edge_fade_z = std::min(edge_z, 1.0f - edge_z) * 4.0f;

      auto edge_fade = std::clamp(
        std::min(edge_fade_x, edge_fade_z), 0.0f, 1.0f);

      height = std::lerp(flat_height, height, edge_fade);

      terrain.set_height(vx, vy, height);
    }
  }
}

} // namespace demo

#endif // DEMO_TERRAIN_GENERATOR_HPP_