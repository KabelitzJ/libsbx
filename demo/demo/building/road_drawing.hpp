// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDING_ROAD_DRAWING_HPP_
#define DEMO_BUILDING_ROAD_DRAWING_HPP_

#include <vector>
#include <cstdint>
#include <cmath>

#include <demo/terrain/chunk.hpp>

namespace demo {

enum class road_snap_mode : std::uint8_t {
  straight_horizontal,
  straight_vertical,
  diagonal,
  l_bend_horizontal_first,
  l_bend_vertical_first,
}; // enum class road_snap_mode

struct road_path {
  std::vector<chunk_coord> cells;
}; // struct road_path

inline auto _build_straight_horizontal(chunk_coord start, chunk_coord end) -> road_path {
  auto path = road_path{};

  auto dx = std::abs(end.x - start.x);
  auto sx = (start.x < end.x) ? 1 : -1;
  auto x = start.x;

  for (auto i = 0; i <= dx; ++i) {
    path.cells.push_back({x, start.y});
    x += sx;
  }

  return path;
}

inline auto _build_straight_vertical(chunk_coord start, chunk_coord end) -> road_path {
  auto path = road_path{};

  auto dy = std::abs(end.y - start.y);
  auto sy = (start.y < end.y) ? 1 : -1;
  auto y = start.y;

  for (auto i = 0; i <= dy; ++i) {
    path.cells.push_back({start.x, y});
    y += sy;
  }

  return path;
}

inline auto _build_diagonal(chunk_coord start, chunk_coord end) -> road_path {
  auto path = road_path{};

  auto dx = std::abs(end.x - start.x);
  auto dy = std::abs(end.y - start.y);
  auto sx = (start.x < end.x) ? 1 : -1;
  auto sy = (start.y < end.y) ? 1 : -1;

  auto diagonal_steps = std::min(dx, dy);
  auto x = start.x;
  auto y = start.y;

  for (auto i = 0; i < diagonal_steps; ++i) {
    path.cells.push_back({x, y});
    x += sx;
    y += sy;
  }

  auto remaining_x = dx - diagonal_steps;
  auto remaining_y = dy - diagonal_steps;

  if (remaining_x > 0) {
    for (auto i = 0; i <= remaining_x; ++i) {
      path.cells.push_back({x, y});
      x += sx;
    }
  } else if (remaining_y > 0) {
    for (auto i = 0; i <= remaining_y; ++i) {
      path.cells.push_back({x, y});
      y += sy;
    }
  } else {
    path.cells.push_back({x, y});
  }

  return path;
}

inline auto _build_l_bend_horizontal_first(chunk_coord start, chunk_coord end) -> road_path {
  auto path = road_path{};

  auto dx = std::abs(end.x - start.x);
  auto dy = std::abs(end.y - start.y);
  auto sx = (start.x < end.x) ? 1 : -1;
  auto sy = (start.y < end.y) ? 1 : -1;

  auto x = start.x;

  for (auto i = 0; i <= dx; ++i) {
    path.cells.push_back({x, start.y});
    x += sx;
  }

  auto y = start.y + sy;

  for (auto i = 1; i <= dy; ++i) {
    path.cells.push_back({end.x, y});
    y += sy;
  }

  return path;
}

inline auto _build_l_bend_vertical_first(chunk_coord start, chunk_coord end) -> road_path {
  auto path = road_path{};

  auto dx = std::abs(end.x - start.x);
  auto dy = std::abs(end.y - start.y);
  auto sx = (start.x < end.x) ? 1 : -1;
  auto sy = (start.y < end.y) ? 1 : -1;

  auto y = start.y;

  for (auto i = 0; i <= dy; ++i) {
    path.cells.push_back({start.x, y});
    y += sy;
  }

  auto x = start.x + sx;

  for (auto i = 1; i <= dx; ++i) {
    path.cells.push_back({x, end.y});
    x += sx;
  }

  return path;
}

inline auto build_road_path(chunk_coord start, chunk_coord end, road_snap_mode mode) -> road_path {
  switch (mode) {
    case road_snap_mode::straight_horizontal: {
      return _build_straight_horizontal(start, end);
    }
    case road_snap_mode::straight_vertical: {
      return _build_straight_vertical(start, end);
    }
    case road_snap_mode::diagonal: {
      return _build_diagonal(start, end);
    }
    case road_snap_mode::l_bend_horizontal_first: {
      return _build_l_bend_horizontal_first(start, end);
    }
    case road_snap_mode::l_bend_vertical_first: {
      return _build_l_bend_vertical_first(start, end);
    }
  }

  return _build_straight_horizontal(start, end);
}

inline auto detect_snap_mode(chunk_coord start, chunk_coord end, bool shift_held) -> road_snap_mode {
  auto dx = std::abs(end.x - start.x);
  auto dy = std::abs(end.y - start.y);

  if (dx == 0 && dy == 0) {
    return road_snap_mode::straight_horizontal;
  }

  if (dx == 0) {
    return road_snap_mode::straight_vertical;
  }

  if (dy == 0) {
    return road_snap_mode::straight_horizontal;
  }

  auto angle_degrees = std::atan2(static_cast<std::float_t>(dy), static_cast<std::float_t>(dx)) * 180.0f / 3.14159265f;

  if (angle_degrees < 22.5f) {
    return road_snap_mode::straight_horizontal;
  }

  if (angle_degrees > 67.5f) {
    return road_snap_mode::straight_vertical;
  }

  if (shift_held) {
    if (dx >= dy) {
      return road_snap_mode::l_bend_horizontal_first;
    }

    return road_snap_mode::l_bend_vertical_first;
  }

  return road_snap_mode::diagonal;
}

inline auto build_snapped_road_path(chunk_coord start, chunk_coord end, bool shift_held) -> road_path {
  auto mode = detect_snap_mode(start, end, shift_held);

  return build_road_path(start, end, mode);
}

} // namespace demo

#endif // DEMO_BUILDING_ROAD_DRAWING_HPP_
