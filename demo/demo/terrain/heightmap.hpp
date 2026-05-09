// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_HEIGHTMAP_HPP_
#define DEMO_TERRAIN_HEIGHTMAP_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <demo/terrain/terrain_types.hpp>

namespace demo {

class heightmap {

public:

  heightmap(const std::filesystem::path& path, std::uint32_t vertices_x, std::uint32_t vertices_z, std::float_t cell_size, std::float_t height_scale)
  : _vertices_x{vertices_x},
    _vertices_z{vertices_z},
    _cell_size{cell_size},
    _height_scale{height_scale},
    _is_dirty{true} {
    auto file = std::ifstream{path, std::ios::binary};

    if (!file) {
      throw std::runtime_error{"failed to open heightmap: " + path.string()};
    }

    auto count = static_cast<std::size_t>(vertices_x) * static_cast<std::size_t>(vertices_z);

    auto raw = std::vector<std::uint16_t>{};
    raw.resize(count);

    file.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(count * sizeof(std::uint16_t)));

    if (!file) {
      throw std::runtime_error{"failed to read heightmap: " + path.string()};
    }

    _heights.resize(count);

    auto inverse_max = 1.0f / static_cast<std::float_t>(std::numeric_limits<std::uint16_t>::max());

    for (auto i = std::size_t{0}; i < count; ++i) {
      auto normalized = static_cast<std::float_t>(raw[i]) * inverse_max;
      _heights[i] = normalized * _height_scale;
    }
  }

  auto get_height(std::int32_t vertex_x, std::int32_t vertex_z) const -> std::float_t {
    auto clamped_x = static_cast<std::uint32_t>(std::clamp(vertex_x, 0, static_cast<std::int32_t>(_vertices_x - 1)));
    auto clamped_z = static_cast<std::uint32_t>(std::clamp(vertex_z, 0, static_cast<std::int32_t>(_vertices_z - 1)));

    return _heights[clamped_z * _vertices_x + clamped_x];
  }

  auto get_height_at(const world_coordinates& coordinates) const -> std::float_t {
    auto grid_x = (coordinates.x + _offset_x()) / _cell_size;
    auto grid_z = (coordinates.z + _offset_z()) / _cell_size;

    auto x0 = static_cast<std::int32_t>(std::floor(grid_x));
    auto z0 = static_cast<std::int32_t>(std::floor(grid_z));
    auto x1 = x0 + 1;
    auto z1 = z0 + 1;

    auto fraction_x = grid_x - std::floor(grid_x);
    auto fraction_z = grid_z - std::floor(grid_z);

    auto h_top    = std::lerp(get_height(x0, z0), get_height(x1, z0), fraction_x);
    auto h_bottom = std::lerp(get_height(x0, z1), get_height(x1, z1), fraction_x);

    return std::lerp(h_top, h_bottom, fraction_z);
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

  auto vertices_x() const -> std::uint32_t {
    return _vertices_x;
  }

  auto vertices_z() const -> std::uint32_t {
    return _vertices_z;
  }

  auto cell_size() const -> std::float_t {
    return _cell_size;
  }

  auto height_scale() const -> std::float_t {
    return _height_scale;
  }

private:

  auto _offset_x() const -> std::float_t {
    return static_cast<std::float_t>(_vertices_x - 1u) * _cell_size * 0.5f;
  }

  auto _offset_z() const -> std::float_t {
    return static_cast<std::float_t>(_vertices_z - 1u) * _cell_size * 0.5f;
  }

  std::uint32_t _vertices_x;
  std::uint32_t _vertices_z;
  std::float_t _cell_size;
  std::float_t _height_scale;
  std::vector<std::float_t> _heights;
  bool _is_dirty;

}; // class heightmap

} // namespace demo

#endif // DEMO_TERRAIN_HEIGHTMAP_HPP_
