// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_HEIGHTMAP_HPP_
#define DEMO_TERRAIN_HEIGHTMAP_HPP_

#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include <libsbx/math/noise.hpp>

#include <demo/terrain/chunk.hpp>

namespace demo {

struct terrain_generation_params {
  float base_scale{0.004f};
  float height_scale{30.0f};
  float valley_depth{5.0f};
  float flatness{0.5f};
  float river_width{0.015f};
  float plateau_threshold{0.6f};
  std::uint32_t octaves{4u};
  float seed_x{0.0f};
  float seed_z{0.0f};
}; // struct terrain_generation_params

class heightmap {

public:

  heightmap(std::uint32_t world_w, std::uint32_t world_h, const terrain_generation_params& params)
  : _world_width{world_w},
    _world_height{world_h},
    _params{params} { }

  auto get_height(std::int32_t vx, std::int32_t vy) const -> float {
    auto cc = _to_chunk_coord(vx, vy);

    auto it = _chunks.find(cc);

    if (it != _chunks.end()) {
      auto lx = static_cast<std::uint32_t>(vx - cc.x * static_cast<std::int32_t>(chunk_size));
      auto ly = static_cast<std::uint32_t>(vy - cc.y * static_cast<std::int32_t>(chunk_size));
      return it->second.at(lx, ly);
    }

    return _generate_height(vx, vy);
  }

  auto set_height(std::int32_t vx, std::int32_t vy, std::float_t h) -> void {
    auto cc = _to_chunk_coord(vx, vy);
    auto& chunk = _get_or_create_chunk(cc);

    auto lx = static_cast<std::uint32_t>(vx - cc.x * static_cast<std::int32_t>(chunk_size));
    auto ly = static_cast<std::uint32_t>(vy - cc.y * static_cast<std::int32_t>(chunk_size));

    chunk.at(lx, ly) = h;
    chunk.is_dirty = true;

    auto dx = (lx == 0) ? -1 : (lx == chunk_size) ? 1 : 0;
    auto dy = (ly == 0) ? -1 : (ly == chunk_size) ? 1 : 0;

    if (dx == 0 && dy == 0) {
      return;
    }

    auto mirror_x = (dx == -1) ? chunk_size : 0u;
    auto mirror_y = (dy == -1) ? chunk_size : 0u;

    if (dx != 0) {
      if (auto* neighbor = _find_chunk({cc.x + dx, cc.y})) {
        neighbor->at(mirror_x, ly) = h;
        neighbor->is_dirty = true;
      }
    }

    if (dy != 0) {
      if (auto* neighbor = _find_chunk({cc.x, cc.y + dy})) {
        neighbor->at(lx, mirror_y) = h;
        neighbor->is_dirty = true;
      }
    }

    if (dx != 0 && dy != 0) {
      if (auto* neighbor = _find_chunk({cc.x + dx, cc.y + dy})) {
        neighbor->at(mirror_x, mirror_y) = h;
        neighbor->is_dirty = true;
      }
    }
  }

  auto get_height_at(float world_x, float world_z) const -> float {
    auto gx = (world_x + _offset_x()) / cell_size();
    auto gz = (world_z + _offset_z()) / cell_size();

    auto x0 = static_cast<std::int32_t>(std::floor(gx));
    auto z0 = static_cast<std::int32_t>(std::floor(gz));
    auto x1 = x0 + 1;
    auto z1 = z0 + 1;

    auto fx = gx - std::floor(gx);
    auto fz = gz - std::floor(gz);

    auto h0 = std::lerp(get_height(x0, z0), get_height(x1, z0), fx);
    auto h1 = std::lerp(get_height(x0, z1), get_height(x1, z1), fx);

    return std::lerp(h0, h1, fz);
  }

  auto has_chunk(chunk_coord cc) const -> bool {
    return _chunks.contains(cc);
  }

  auto get_chunk(chunk_coord cc) -> height_chunk* {
    auto it = _chunks.find(cc);
    return (it != _chunks.end()) ? &it->second : nullptr;
  }

  auto get_chunk(chunk_coord cc) const -> const height_chunk* {
    auto it = _chunks.find(cc);
    return (it != _chunks.end()) ? &it->second : nullptr;
  }

  auto ensure_chunk(chunk_coord cc) -> height_chunk& {
    return _get_or_create_chunk(cc);
  }

  auto active_chunk_count() const -> std::size_t {
    return _chunks.size();
  }

  auto world_width() const -> std::uint32_t {
    return _world_width;
  }

  auto world_height() const -> std::uint32_t {
    return _world_height;
  }

  auto verts_w() const -> std::uint32_t {
    return _world_width + 1;
  }

  auto verts_h() const -> std::uint32_t {
    return _world_height + 1;
  }

  auto params() const -> const terrain_generation_params& {
    return _params;
  }

  static auto cell_size() -> float {
    return grid::cell_size;
  }

  template<typename Callable>
  auto for_each_chunk(Callable&& callable) -> void {
    for (auto& [coord, chunk] : _chunks) {
      std::invoke(callable, coord, chunk);
    }
  }

  template<typename Callable>
  auto for_each_chunk(Callable&& callable) const -> void {
    for (const auto& [coord, chunk] : _chunks) {
      std::invoke(callable, coord, chunk);
    }
  }

private:

  auto _find_chunk(chunk_coord cc) -> height_chunk* {
    auto it = _chunks.find(cc);

    return (it != _chunks.end()) ? &it->second : nullptr;
  }

  auto _offset_x() const -> float {
    return static_cast<float>(_world_width) * grid::cell_size * 0.5f;
  }

  auto _offset_z() const -> float {
    return static_cast<float>(_world_height) * grid::cell_size * 0.5f;
  }

  auto _to_chunk_coord(std::int32_t vx, std::int32_t vy) const -> chunk_coord {
    auto fx = static_cast<float>(vx) / static_cast<float>(chunk_size);
    auto fy = static_cast<float>(vy) / static_cast<float>(chunk_size);

    return chunk_coord{static_cast<std::int32_t>(std::floor(fx)), static_cast<std::int32_t>(std::floor(fy))};
  }

  auto _generate_height(std::int32_t vx, std::int32_t vy) const -> float {
    auto wx = static_cast<float>(vx) + _params.seed_x;
    auto wz = static_cast<float>(vy) + _params.seed_z;

    auto base = sbx::math::noise::fractal(wx * _params.base_scale, wz * _params.base_scale, _params.octaves);

    auto mountains = sbx::math::noise::fractal(wx * _params.base_scale * 0.5f + 100.0f, wz * _params.base_scale * 0.5f + 100.0f, 3u);

    auto detail = sbx::math::noise::fractal(wx * _params.base_scale * 4.0f + 200.0f, wz * _params.base_scale * 4.0f + 200.0f, 2u);

    auto river_noise = sbx::math::noise::fractal(wx * _params.river_width + 300.0f, wz * _params.river_width + 300.0f, 3u);

    auto river_carved = 1.0f - std::abs(river_noise);
    river_carved = river_carved * river_carved;

    auto shaped = base;

    if (shaped > _params.plateau_threshold) {
      auto excess = shaped - _params.plateau_threshold;

      shaped = _params.plateau_threshold + excess * 0.2f;
    }

    auto height = shaped * _params.height_scale + mountains * _params.height_scale * 0.4f + detail * 0.8f - (1.0f - river_carved) * _params.valley_depth;

    auto flat_height = _params.height_scale * 0.2f;
  
    height = std::lerp(height, flat_height, _params.flatness);

    auto total_verts_w = static_cast<float>(_world_width);
    auto total_verts_h = static_cast<float>(_world_height);

    auto edge_x = static_cast<float>(vx) / total_verts_w;
    auto edge_z = static_cast<float>(vy) / total_verts_h;

    auto edge_fade_x = std::min(edge_x, 1.0f - edge_x) * 4.0f;
    auto edge_fade_z = std::min(edge_z, 1.0f - edge_z) * 4.0f;

    auto edge_fade = std::clamp(std::min(edge_fade_x, edge_fade_z), 0.0f, 1.0f);

    height = std::lerp(flat_height, height, edge_fade);

    return height;
  }

  auto _get_or_create_chunk(chunk_coord cc) -> height_chunk& {
    auto it = _chunks.find(cc);

    if (it != _chunks.end()) {
      return it->second;
    }

    auto& chunk = _chunks[cc];

    for (auto ly = 0u; ly < chunk_verts; ++ly) {
      for (auto lx = 0u; lx < chunk_verts; ++lx) {
        auto gx = cc.x * static_cast<std::int32_t>(chunk_size) + static_cast<std::int32_t>(lx);
        auto gy = cc.y * static_cast<std::int32_t>(chunk_size) + static_cast<std::int32_t>(ly);

        chunk.at(lx, ly) = _generate_height(gx, gy);
      }
    }

    if (auto* left = _find_chunk({cc.x - 1, cc.y})) {
      for (auto ly = 0u; ly < chunk_verts; ++ly) {
        chunk.at(0, ly) = left->at(chunk_size, ly);
      }
    }

    if (auto* right = _find_chunk({cc.x + 1, cc.y})) {
      for (auto ly = 0u; ly < chunk_verts; ++ly) {
        chunk.at(chunk_size, ly) = right->at(0, ly);
      }
    }

    if (auto* top = _find_chunk({cc.x, cc.y - 1})) {
      for (auto lx = 0u; lx < chunk_verts; ++lx) {
        chunk.at(lx, 0) = top->at(lx, chunk_size);
      }
    }

    if (auto* bottom = _find_chunk({cc.x, cc.y + 1})) {
      for (auto lx = 0u; lx < chunk_verts; ++lx) {
        chunk.at(lx, chunk_size) = bottom->at(lx, 0);
      }
    }

    if (auto* tl = _find_chunk({cc.x - 1, cc.y - 1})) {
      chunk.at(0, 0) = tl->at(chunk_size, chunk_size);
    }

    if (auto* tr = _find_chunk({cc.x + 1, cc.y - 1})) {
      chunk.at(chunk_size, 0) = tr->at(0, chunk_size);
    }

    if (auto* bl = _find_chunk({cc.x - 1, cc.y + 1})) {
      chunk.at(0, chunk_size) = bl->at(chunk_size, 0);
    }

    if (auto* br = _find_chunk({cc.x + 1, cc.y + 1})) {
      chunk.at(chunk_size, chunk_size) = br->at(0, 0);
    }

    chunk.is_dirty = true;

    return chunk;
  }

  std::uint32_t _world_width{};
  std::uint32_t _world_height{};
  terrain_generation_params _params;
  std::unordered_map<chunk_coord, height_chunk, chunk_coord_hash> _chunks;

}; // class heightmap

} // namespace demo

#endif // DEMO_TERRAIN_HEIGHTMAP_HPP_