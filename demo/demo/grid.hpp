// SPDX-License-Identifier: MIT
#ifndef DEMO_GRID_HPP_
#define DEMO_GRID_HPP_

#include <vector>

namespace demo {

struct cell {
  std::uint16_t building_id{0};
  std::uint8_t terrain_type{0};
  std::uint8_t zone_type{0};
  std::uint8_t flags{0};
  std::uint16_t district_id{0};
}; // struct cell

struct grid_offset {

  std::int32_t x{};
  std::int32_t y{};

  auto operator==(const grid_offset& other) const -> bool = default;

}; // struct grid_offset

class grid {

public:

  static constexpr auto cell_size = 2.0f;

  grid(std::uint32_t width, std::uint32_t height)
  : _width{width},
    _height{height},
    _cells() {
    _cells.resize(_width * _height);
  }

  auto at(std::uint32_t x, std::uint32_t y) -> cell& {
    return _cells[y * _width + x];
  }

  auto at(std::uint32_t x, std::uint32_t y) const -> const cell& {
    return _cells[y * _width + x];
  }

  auto in_bounds(std::uint32_t x, std::uint32_t y) const -> bool {
    return x < _width && y < _height;
  }

  auto width() const -> std::uint32_t {
    return _width;
  }

  auto height() const -> std::uint32_t {
    return _height;
  }

private:

  std::uint32_t _width{};
  std::uint32_t _height{};
  std::vector<cell> _cells;

}; // class grid

} // namespace demo

#endif // DEMO_GRID_HPP_