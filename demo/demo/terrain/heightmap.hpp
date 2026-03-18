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
  std::float_t base_scale{0.003f};
  std::float_t height_scale{30.0f};
  std::float_t river_width{0.003f};
  std::uint32_t octaves{3u};
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

  auto _generate_height(std::int32_t vertex_x, std::int32_t vertex_z) const -> float {
    auto world_x = static_cast<float>(vertex_x) + _params.seed_x;
    auto world_z = static_cast<float>(vertex_z) + _params.seed_z;

    // Plains base
    auto plains_noise = sbx::math::noise::fractal(world_x * _params.base_scale, world_z * _params.base_scale, _params.octaves);
    auto plains_height = terrain_constants::sea_level + 2.5f + plains_noise * 2.5f;

    // Mountain ranges
    auto mountain_mask_noise = sbx::math::noise::fractal(world_x * _params.base_scale * 0.5f + 100.0f, world_z * _params.base_scale * 0.5f + 100.0f, 2u);
    auto mountain_mask = std::clamp((mountain_mask_noise - 0.4f) / 0.5f, 0.0f, 1.0f);
    mountain_mask = mountain_mask * mountain_mask;

    auto mountain_shape = sbx::math::noise::fractal(world_x * _params.base_scale * 2.0f + 200.0f, world_z * _params.base_scale * 2.0f + 200.0f, 3u);

    auto ridge_noise = sbx::math::noise::fractal(world_x * _params.base_scale * 6.0f + 600.0f, world_z * _params.base_scale * 6.0f + 600.0f, 3u);
    auto ridge = 1.0f - std::abs(ridge_noise);
    ridge = ridge * ridge;

    auto mountain_detail = mountain_shape * 0.4f + ridge * 0.6f;
    auto mountain_height = mountain_mask * mountain_detail * _params.height_scale;

    // Rivers
    auto river_noise = sbx::math::noise::fractal(world_x * _params.river_width + 300.0f, world_z * _params.river_width + 300.0f, 2u);
    auto river_proximity = 1.0f - std::clamp(std::abs(river_noise) / 0.12f, 0.0f, 1.0f);

    auto river_carve = river_proximity * (terrain_constants::sea_level - terrain_constants::river_bed_min);

    // Lakes
    auto lake_noise = sbx::math::noise::simplex(world_x * _params.base_scale * 1.5f + 400.0f, world_z * _params.base_scale * 1.5f + 400.0f);
    auto lake_factor = std::clamp((-lake_noise - 0.45f) / 0.35f, 0.0f, 1.0f);

    auto lake_carve = lake_factor * (terrain_constants::sea_level - terrain_constants::lake_bed_min);

    // Fine detail
    auto detail = sbx::math::noise::fractal(world_x * _params.base_scale * 10.0f + 500.0f, world_z * _params.base_scale * 10.0f + 500.0f, 2u);

    // Combine
    auto height = plains_height + mountain_height + detail * 0.2f;

    auto carve_mask = 1.0f - mountain_mask;
    height -= river_carve * carve_mask;
    height -= lake_carve * carve_mask;

    // Edge falloff
    auto total_vertices_x = static_cast<float>(_world_width);
    auto total_vertices_z = static_cast<float>(_world_height);

    auto edge_x = static_cast<float>(vertex_x) / total_vertices_x;
    auto edge_z = static_cast<float>(vertex_z) / total_vertices_z;

    auto edge_fade_x = std::min(edge_x, 1.0f - edge_x) * 4.0f;
    auto edge_fade_z = std::min(edge_z, 1.0f - edge_z) * 4.0f;

    auto edge_fade = std::clamp(std::min(edge_fade_x, edge_fade_z), 0.0f, 1.0f);

    auto edge_height = terrain_constants::sea_level + 2.0f;
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
