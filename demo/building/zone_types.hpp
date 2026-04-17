// SPDX-License-Identifier: MIT
#ifndef DEMO_BUILDING_ZONE_TYPES_HPP_
#define DEMO_BUILDING_ZONE_TYPES_HPP_

#include <cstdint>
#include <cmath>
#include <array>

namespace demo {

enum class zone_type : std::uint8_t {
  none = 0,
  residential = 1,
  industrial = 2,
  extraction = 3,
  agricultural = 4,
  civic = 5,
  military = 6,
  infrastructure = 7,
}; // enum class zone_type

static constexpr auto zone_type_count = 7u;

struct zone_color {
  std::float_t r;
  std::float_t g;
  std::float_t b;
}; // struct zone_color

inline auto get_zone_color(zone_type type) -> zone_color {
  switch (type) {
    case zone_type::residential:    return {0.2f, 0.7f, 0.3f};
    case zone_type::industrial:     return {0.8f, 0.6f, 0.1f};
    case zone_type::extraction:     return {0.6f, 0.4f, 0.2f};
    case zone_type::agricultural:   return {0.4f, 0.8f, 0.2f};
    case zone_type::civic:          return {0.3f, 0.4f, 0.9f};
    case zone_type::military:       return {0.7f, 0.2f, 0.2f};
    case zone_type::infrastructure: return {0.5f, 0.5f, 0.5f};
    default:                        return {0.0f, 0.0f, 0.0f};
  }
}

inline auto get_zone_name(zone_type type) -> const char* {
  switch (type) {
    case zone_type::residential:    return "residential";
    case zone_type::industrial:     return "industrial";
    case zone_type::extraction:     return "extraction";
    case zone_type::agricultural:   return "agricultural";
    case zone_type::civic:          return "civic";
    case zone_type::military:       return "military";
    case zone_type::infrastructure: return "infrastructure";
    default:                        return "none";
  }
}

} // namespace demo

#endif // DEMO_BUILDING_ZONE_TYPES_HPP_
