// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_HPP_
#define DEMO_TERRAIN_HPP_

#include <vector>

#include <demo/grid.hpp>

namespace demo {

class terrain {

public:

  terrain(std::uint32_t grid_w, std::uint32_t grid_h)
  : _verts_w{grid_w + 1},
    _verts_h{grid_h + 1},
    _height() {
    _height.resize(_verts_w * _verts_h, 0.0f);
  }

  auto get_height(std::uint32_t vx, std::uint32_t vy) const -> std::float_t {
    return _height[vy * _verts_w + vx];
  }

  auto set_height(std::uint32_t vx, std::uint32_t vy, std::float_t h) -> void {
    _height[vy * _verts_w + vx] = h;
  }

  auto offset_x() const -> float {
    return static_cast<float>(_verts_w - 1) * grid::cell_size * 0.5f;
  }

  auto offset_z() const -> float {
    return static_cast<float>(_verts_h - 1) * grid::cell_size * 0.5f;
  }

  auto get_height_at(std::float_t world_x, std::float_t world_z) const -> std::float_t {
    auto gx = (world_x + offset_x()) / grid::cell_size;
    auto gz = (world_z + offset_z()) / grid::cell_size;

    auto x0 = std::clamp(static_cast<std::uint32_t>(std::floor(gx)), 0u, _verts_w - 1);
    auto z0 = std::clamp(static_cast<std::uint32_t>(std::floor(gz)), 0u, _verts_h - 1);
    auto x1 = std::clamp(x0 + 1, 0u, _verts_w - 1);
    auto z1 = std::clamp(z0 + 1, 0u, _verts_h - 1);

    auto fx = gx - std::floor(gx);
    auto fz = gz - std::floor(gz);

    auto h0 = std::lerp(get_height(x0, z0), get_height(x1, z0), fx);
    auto h1 = std::lerp(get_height(x0, z1), get_height(x1, z1), fx);

    return std::lerp(h0, h1, fz);
  }

  auto verts_w() const -> std::uint32_t {
    return _verts_w;
  }

  auto verts_h() const -> std::uint32_t {
    return _verts_h;
  }

private:

  std::uint32_t _verts_w{};
  std::uint32_t _verts_h{};
  std::vector<std::float_t> _height;

}; // class terrain

} // namespace demo

#endif // DEMO_TERRAIN_HPP_