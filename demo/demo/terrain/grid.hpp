// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_GRID_HPP_
#define DEMO_TERRAIN_GRID_HPP_

#include <unordered_map>
#include <cstdint>
#include <cmath>

#include <demo/terrain/chunk.hpp>

namespace demo {

class grid {

public:

  static constexpr auto cell_size = 2.0f;

  grid(std::uint32_t world_width, std::uint32_t world_height)
  : _world_width{world_width},
    _world_height{world_height} { }

  auto at(std::int32_t world_x, std::int32_t world_y) -> cell& {
    auto chunk_coordinates = _to_chunk_coord(world_x, world_y);
    auto& chunk = _get_or_create_chunk(chunk_coordinates);

    auto local_x = static_cast<std::uint32_t>(world_x - chunk_coordinates.x * static_cast<std::int32_t>(chunk_size));
    auto local_y = static_cast<std::uint32_t>(world_y - chunk_coordinates.y * static_cast<std::int32_t>(chunk_size));

    return chunk.at(local_x, local_y);
  }

  auto at(std::int32_t world_x, std::int32_t world_y) const -> const cell& {
    static constexpr auto empty_cell = cell{};

    auto chunk_coordinates = _to_chunk_coord(world_x, world_y);
    auto chunk_iterator = _chunks.find(chunk_coordinates);

    if (chunk_iterator == _chunks.end()) {
      return empty_cell;
    }

    auto local_x = static_cast<std::uint32_t>(world_x - chunk_coordinates.x * static_cast<std::int32_t>(chunk_size));

    auto local_y = static_cast<std::uint32_t>(world_y - chunk_coordinates.y * static_cast<std::int32_t>(chunk_size));

    return chunk_iterator->second.at(local_x, local_y);
  }

  auto in_bounds(std::int32_t world_x, std::int32_t world_y) const -> bool {
    return world_x >= 0 && world_y >= 0 && world_x < static_cast<std::int32_t>(_world_width) && world_y < static_cast<std::int32_t>(_world_height);
  }

  auto has_chunk(chunk_coord chunk_coordinates) const -> bool {
    return _chunks.contains(chunk_coordinates);
  }

  auto get_chunk(chunk_coord chunk_coordinates) -> grid_chunk* {
    auto chunk_iterator = _chunks.find(chunk_coordinates);

    return (chunk_iterator != _chunks.end()) ? &chunk_iterator->second : nullptr;
  }

  auto get_chunk(chunk_coord chunk_coordinates) const -> const grid_chunk* {
    auto chunk_iterator = _chunks.find(chunk_coordinates);

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
    for (auto& [chunk_coordinates, chunk] : _chunks) {
      chunk_callback(chunk_coordinates, chunk);
    }
  }

  template<typename Func>
  auto for_each_chunk(Func&& chunk_callback) const -> void {
    for (const auto& [chunk_coordinates, chunk] : _chunks) {
      chunk_callback(chunk_coordinates, chunk);
    }
  }

private:

  auto _to_chunk_coord(std::int32_t world_x, std::int32_t world_y) const -> chunk_coord {
    auto normalized_chunk_x = static_cast<std::float_t>(world_x) / static_cast<std::float_t>(chunk_size);
    auto normalized_chunk_y = static_cast<std::float_t>(world_y) / static_cast<std::float_t>(chunk_size);

    return chunk_coord{static_cast<std::int32_t>(std::floor(normalized_chunk_x)), static_cast<std::int32_t>(std::floor(normalized_chunk_y))};
  }

  auto _get_or_create_chunk(chunk_coord chunk_coordinates) -> grid_chunk& {
    return _chunks[chunk_coordinates];
  }

  std::uint32_t _world_width{};
  std::uint32_t _world_height{};

  std::unordered_map<chunk_coord, grid_chunk, chunk_coord_hash> _chunks;

}; // class grid

} // namespace demo

#endif // DEMO_TERRAIN_GRID_HPP_
