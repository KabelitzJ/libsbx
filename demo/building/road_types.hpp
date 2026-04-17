// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDING_ROAD_TYPES_HPP_
#define DEMO_BUILDING_ROAD_TYPES_HPP_

#include <cstdint>
#include <cmath>
#include <array>

namespace demo {

enum class road_type : std::uint8_t {
  none = 0,
  dirt = 1,
  gravel = 2,
  paved = 3,
  highway = 4,
}; // enum class road_type

struct road_properties {
  std::float_t half_width;
  std::float_t height_offset;
  std::float_t speed_factor;
  std::float_t build_cost;
}; // struct road_properties

inline auto get_road_properties(road_type type) -> road_properties {
  switch (type) {
    case road_type::dirt:    return {0.8f, 0.02f, 1.0f, 1.0f};
    case road_type::gravel:  return {1.0f, 0.03f, 1.5f, 5.0f};
    case road_type::paved:   return {1.4f, 0.04f, 2.5f, 20.0f};
    case road_type::highway: return {2.2f, 0.05f, 4.0f, 50.0f};
    default:                 return {0.0f, 0.0f, 0.0f, 0.0f};
  }
}

static constexpr auto road_direction_offset_x = std::array<std::int32_t, 8>{0, 1, 1, 1, 0, -1, -1, -1};
static constexpr auto road_direction_offset_z = std::array<std::int32_t, 8>{-1, -1, 0, 1, 1, 1, 0, -1};

} // namespace demo

#endif // DEMO_BUILDING_ROAD_TYPES_HPP_
