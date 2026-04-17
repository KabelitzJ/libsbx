// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_GRID_HPP_
#define DEMO_TERRAIN_GRID_HPP_

#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include <demo/terrain/chunk.hpp>

namespace demo {

class grid {

public:

  static constexpr auto cell_size = 2.0f;

  grid(std::uint32_t world_width, std::uint32_t world_height)
    : _world_width{world_width},
      _world_height{world_height} {
  }

  auto at(const cell_coordinates& coordinates) -> cell& {
    auto chunk_coords = _to_chunk_coordinates(coordinates);
    auto& chunk = _get_or_create_chunk(chunk_coords);

    auto local_x = static_cast<std::uint32_t>(coordinates.x - chunk_coords.x * static_cast<std::int32_t>(chunk_size));
    auto local_z = static_cast<std::uint32_t>(coordinates.z - chunk_coords.z * static_cast<std::int32_t>(chunk_size));

    return chunk.at({static_cast<std::int32_t>(local_x), static_cast<std::int32_t>(local_z)});
  }

  auto at(const cell_coordinates& coordinates) const -> const cell& {
    static constexpr auto empty_cell = cell{};

    auto chunk_coords = _to_chunk_coordinates(coordinates);
    auto chunk_iterator = _chunks.find(chunk_coords);

    if (chunk_iterator == _chunks.end()) {
      return empty_cell;
    }

    auto local_x = static_cast<std::uint32_t>(coordinates.x - chunk_coords.x * static_cast<std::int32_t>(chunk_size));
    auto local_z = static_cast<std::uint32_t>(coordinates.z - chunk_coords.z * static_cast<std::int32_t>(chunk_size));

    return chunk_iterator->second.at({static_cast<std::int32_t>(local_x), static_cast<std::int32_t>(local_z)});
  }

  auto at(std::int32_t x, std::int32_t z) -> cell& {
    return at({x, z});
  }

  auto at(std::int32_t x, std::int32_t z) const -> const cell& {
    return at({x, z});
  }

  auto in_bounds(std::int32_t world_x, std::int32_t world_z) const -> bool {
    return world_x >= 0 && world_z >= 0 && world_x < static_cast<std::int32_t>(_world_width) && world_z < static_cast<std::int32_t>(_world_height);
  }

  auto has_chunk(cell_coordinates cell_coordinates) const -> bool {
    return _chunks.contains(cell_coordinates);
  }

  auto get_chunk(cell_coordinates cell_coordinates) -> grid_chunk* {
    auto chunk_iterator = _chunks.find(cell_coordinates);

    return (chunk_iterator != _chunks.end()) ? &chunk_iterator->second : nullptr;
  }

  auto get_chunk(cell_coordinates cell_coordinates) const -> const grid_chunk* {
    auto chunk_iterator = _chunks.find(cell_coordinates);

    return (chunk_iterator != _chunks.end()) ? &chunk_iterator->second : nullptr;
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

  auto offset_x() const -> std::float_t {
    return static_cast<std::float_t>(_world_width) * cell_size * 0.5f;
  }

  auto offset_z() const -> std::float_t {
    return static_cast<std::float_t>(_world_height) * cell_size * 0.5f;
  }

  template<typename Func>
  auto for_each_chunk(Func&& chunk_callback) -> void {
    for (auto& [cell_coordinates, chunk] : _chunks) {
      chunk_callback(cell_coordinates, chunk);
    }
  }

  template<typename Func>
  auto for_each_chunk(Func&& chunk_callback) const -> void {
    for (const auto& [cell_coordinates, chunk] : _chunks) {
      chunk_callback(cell_coordinates, chunk);
    }
  }

private:

  auto _to_chunk_coordinates(const cell_coordinates& coordinates) const -> cell_coordinates {
    auto normalized_chunk_x = static_cast<std::float_t>(coordinates.x) / static_cast<std::float_t>(chunk_size);
    auto normalized_chunk_z = static_cast<std::float_t>(coordinates.z) / static_cast<std::float_t>(chunk_size);

    return cell_coordinates{static_cast<std::int32_t>(std::floor(normalized_chunk_x)), static_cast<std::int32_t>(std::floor(normalized_chunk_z))};
  }

  auto _get_or_create_chunk(cell_coordinates cell_coordinates) -> grid_chunk& {
    return _chunks[cell_coordinates];
  }

  std::uint32_t _world_width{};
  std::uint32_t _world_height{};

  std::unordered_map<cell_coordinates, grid_chunk, cell_coordinates_hash> _chunks;

};

} // namespace demo

#endif // DEMO_TERRAIN_GRID_HPP_
