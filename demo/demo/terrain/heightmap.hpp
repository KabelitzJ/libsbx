// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_HEIGHTMAP_HPP_
#define DEMO_TERRAIN_HEIGHTMAP_HPP_

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include <libsbx/math/noise.hpp>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/grid.hpp>

namespace demo {

struct terrain_generation_params {
  std::float_t base_scale{0.0015f};
  std::float_t height_scale{30.0f};
  std::float_t warp_scale{0.002f};
  std::float_t warp_strength{80.0f};
  std::float_t continental_bias{0.65f};
  std::uint32_t octaves{4u};
  std::float_t seed_x{0.0f};
  std::float_t seed_z{0.0f};
}; // struct terrain_generation_params

struct terrain_constants {
  static constexpr auto sea_level = 5.0f;
  static constexpr auto river_bed_min = 1.0f;
  static constexpr auto lake_bed_min = 0.5f;
  static constexpr auto shore_max = 6.5f;
  static constexpr auto plains_max = 10.0f;
  static constexpr auto hills_max = 16.0f;
  static constexpr auto mountain_max = 35.0f;
}; // struct terrain_constants

class heightmap {

public:

  heightmap(std::uint32_t world_width, std::uint32_t world_height, const terrain_generation_params& params)
  : _world_width{world_width},
    _world_height{world_height},
    _params{params},
    _is_dirty{true} {
    auto vertex_count_x = vertices_x();
    auto vertex_count_z = vertices_z();

    _heights.resize(vertex_count_x * vertex_count_z);

    for (auto vertex_z = 0u; vertex_z < vertex_count_z; ++vertex_z) {
      for (auto vertex_x = 0u; vertex_x < vertex_count_x; ++vertex_x) {
        _heights[vertex_z * vertex_count_x + vertex_x] = _generate_height(static_cast<std::int32_t>(vertex_x), static_cast<std::int32_t>(vertex_z));
      }
    }
  }

  auto get_height(std::int32_t vertex_x, std::int32_t vertex_z) const -> std::float_t {
    auto clamped_x = static_cast<std::uint32_t>(std::clamp(vertex_x, 0, static_cast<std::int32_t>(vertices_x() - 1)));
    auto clamped_z = static_cast<std::uint32_t>(std::clamp(vertex_z, 0, static_cast<std::int32_t>(vertices_z() - 1)));

    return _heights[clamped_z * vertices_x() + clamped_x];
  }

  auto set_height(std::int32_t vertex_x, std::int32_t vertex_z, std::float_t height) -> void {
    if (vertex_x < 0 || vertex_z < 0 || vertex_x >= static_cast<std::int32_t>(vertices_x()) || vertex_z >= static_cast<std::int32_t>(vertices_z())) {
      return;
    }

    _heights[static_cast<std::uint32_t>(vertex_z) * vertices_x() + static_cast<std::uint32_t>(vertex_x)] = height;
    _is_dirty = true;
  }

  auto get_height_at(std::float_t world_x, std::float_t world_z) const -> std::float_t {
    auto grid_x = (world_x + _offset_x()) / cell_size();
    auto grid_z = (world_z + _offset_z()) / cell_size();

    auto x0 = static_cast<std::int32_t>(std::floor(grid_x));
    auto z0 = static_cast<std::int32_t>(std::floor(grid_z));
    auto x1 = x0 + 1;
    auto z1 = z0 + 1;

    auto fraction_x = grid_x - std::floor(grid_x);
    auto fraction_z = grid_z - std::floor(grid_z);

    auto height_top = std::lerp(get_height(x0, z0), get_height(x1, z0), fraction_x);
    auto height_bottom = std::lerp(get_height(x0, z1), get_height(x1, z1), fraction_x);

    return std::lerp(height_top, height_bottom, fraction_z);
  }

  auto data() const -> const std::float_t* {
    return _heights.data();
  }

  auto data_size_bytes() const -> std::size_t {
    return _heights.size() * sizeof(std::float_t);
  }

  auto is_dirty() const -> bool {
    return _is_dirty;
  }

  auto clear_dirty() -> void {
    _is_dirty = false;
  }

  auto world_width() const -> std::uint32_t {
    return _world_width;
  }

  auto world_height() const -> std::uint32_t {
    return _world_height;
  }

  auto vertices_x() const -> std::uint32_t {
    return _world_width + 1;
  }

  auto vertices_z() const -> std::uint32_t {
    return _world_height + 1;
  }

  auto params() const -> const terrain_generation_params& {
    return _params;
  }

  static auto cell_size() -> std::float_t {
    return grid::cell_size;
  }

private:

  auto _offset_x() const -> std::float_t {
    return static_cast<std::float_t>(_world_width) * grid::cell_size * 0.5f;
  }

  auto _offset_z() const -> std::float_t {
    return static_cast<std::float_t>(_world_height) * grid::cell_size * 0.5f;
  }

  auto _ridged_multifractal(float x, float z, std::uint32_t octaves) const -> float {
    auto sum = 0.0f;
    auto frequency = 1.0f;
    auto amplitude = 1.0f;
    auto weight = 1.0f;

    for (auto i = 0u; i < octaves; ++i) {
      auto signal = sbx::math::noise::simplex(x * frequency, z * frequency);
      signal = 1.0f - std::abs(signal);
      signal *= signal;
      signal *= weight;
      weight = std::clamp(signal * 2.0f, 0.0f, 1.0f);

      sum += signal * amplitude;
      frequency *= 2.2f;
      amplitude *= 0.45f;
    }

    return sum;
  }

  auto _generate_height(std::int32_t vertex_x, std::int32_t vertex_z) const -> float {
    auto wx = static_cast<float>(vertex_x) + _params.seed_x;
    auto wz = static_cast<float>(vertex_z) + _params.seed_z;

    // Domain warp for organic shapes
    auto warp_x = sbx::math::noise::fractal(wx * _params.warp_scale, wz * _params.warp_scale, 2u) * _params.warp_strength;
    auto warp_z = sbx::math::noise::fractal(wx * _params.warp_scale + 100.0f, wz * _params.warp_scale + 100.0f, 2u) * _params.warp_strength;

    auto warped_x = wx + warp_x;
    auto warped_z = wz + warp_z;

    // Continental shape — controls land vs sea boundary
    auto continental = sbx::math::noise::fractal(warped_x * _params.base_scale, warped_z * _params.base_scale, 3u);
    continental = continental * 0.5f + _params.continental_bias;
    continental = std::clamp(continental, 0.0f, 1.0f);

    // Double smoothstep for steep coastlines — pushes values away from the sea level threshold
    continental = continental * continental * (3.0f - 2.0f * continental);
    continental = continental * continental * (3.0f - 2.0f * continental);

    auto base_height = continental * 12.0f;

    // Mountain mask — broad regions where mountains can exist
    auto mountain_noise = sbx::math::noise::fractal(warped_x * _params.base_scale * 0.4f + 300.0f, warped_z * _params.base_scale * 0.4f + 300.0f, 2u);
    auto mountain_mask = std::clamp((mountain_noise - 0.15f) / 0.55f, 0.0f, 1.0f);
    mountain_mask *= mountain_mask;

    // Mountains only on solid land
    auto land_factor = std::clamp((continental - 0.45f) / 0.25f, 0.0f, 1.0f);
    mountain_mask *= land_factor;

    // Ridged component — sharp crests and valleys
    auto ridge = _ridged_multifractal(warped_x * _params.base_scale * 1.5f + 200.0f, warped_z * _params.base_scale * 1.5f + 200.0f, _params.octaves);

    // Massif component — broad rounded domes
    auto massif_raw = sbx::math::noise::fractal(warped_x * _params.base_scale * 0.6f + 800.0f, warped_z * _params.base_scale * 0.6f + 800.0f, 3u);
    auto massif = std::clamp(massif_raw * 0.5f + 0.5f, 0.0f, 1.0f);
    massif = massif * massif; // squaring rounds off the shape further

    // Blend: where massif noise is strong, use the dome shape;
    // where it's weak, the ridges show through.
    auto blend_noise = sbx::math::noise::simplex(warped_x * _params.base_scale * 1.2f + 400.0f, warped_z * _params.base_scale * 1.2f + 400.0f);
    auto ridge_weight = std::clamp(blend_noise * 0.5f + 0.5f, 0.25f, 0.75f);

    auto mountain_shape = std::lerp(massif * 1.3f, ridge, ridge_weight);
    auto mountain_height = mountain_mask * mountain_shape * _params.height_scale;

    // Rolling hills — medium freq, gentle
    auto hills = sbx::math::noise::fractal(warped_x * _params.base_scale * 5.0f + 500.0f, warped_z * _params.base_scale * 5.0f + 500.0f, 3u);
    auto hill_strength = std::clamp(continental - 0.35f, 0.0f, 1.0f) * (1.0f - mountain_mask * 0.8f);
    auto hill_contribution = hills * 2.5f * hill_strength;

    // Fine detail noise
    auto detail = sbx::math::noise::fractal(warped_x * _params.base_scale * 20.0f + 700.0f, warped_z * _params.base_scale * 20.0f + 700.0f, 2u);
    auto detail_contribution = detail * 0.3f;

    auto height = base_height + mountain_height + hill_contribution + detail_contribution;

    // Edge falloff
    auto total_vertices_x = static_cast<float>(_world_width);
    auto total_vertices_z = static_cast<float>(_world_height);

    auto edge_x = static_cast<float>(vertex_x) / total_vertices_x;
    auto edge_z = static_cast<float>(vertex_z) / total_vertices_z;

    auto edge_fade_x = std::min(edge_x, 1.0f - edge_x) * 4.0f;
    auto edge_fade_z = std::min(edge_z, 1.0f - edge_z) * 4.0f;

    auto edge_fade = std::clamp(std::min(edge_fade_x, edge_fade_z), 0.0f, 1.0f);

    auto edge_height = terrain_constants::sea_level - 1.0f;
    height = std::lerp(edge_height, height, edge_fade);

    return height;
  }

  std::uint32_t _world_width{};
  std::uint32_t _world_height{};
  terrain_generation_params _params;
  std::vector<std::float_t> _heights;
  bool _is_dirty{true};

}; // class heightmap

} // namespace demo

#endif // DEMO_TERRAIN_HEIGHTMAP_HPP_
