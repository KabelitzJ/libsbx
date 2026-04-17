// SPDX-License-Identifier: MIT
#ifndef DEMO_TOWN_HPP_
#define DEMO_TOWN_HPP_

#include <string>
#include <vector>
#include <unordered_set>
#include <cstdint>
#include <cmath>

#include <demo/terrain/chunk.hpp>

namespace demo {

enum class path_category : std::uint8_t {
  none = 0,
  road = 1,
  rail = 2,
}; // enum class path_category

enum class road_type : std::uint8_t {
  none = 0,
  dirt = 1,
  gravel = 2,
  paved = 3,
  highway = 4,
}; // enum class road_type

enum class rail_type : std::uint8_t {
  none = 0,
  standard = 1,
  heavy = 2,
}; // enum class rail_type

struct path_properties {
  std::uint8_t width;
  std::float_t speed_factor;
  std::float_t build_cost;
  std::float_t maintenance;
}; // struct path_properties

using town_id = std::uint16_t;

static constexpr auto invalid_town_id = town_id{0};

struct town_resources {
  std::float_t coal{0.0f};
  std::float_t iron{0.0f};
  std::float_t wood{0.0f};
  std::float_t food{0.0f};
  std::float_t steel{0.0f};
  std::float_t concrete{0.0f};
  std::float_t fuel{0.0f};
  std::float_t prefab_panels{0.0f};
  std::float_t textiles{0.0f};
  std::float_t rubles{0.0f};
}; // struct town_resources

struct town_stats {
  std::uint32_t population{0};
  std::uint32_t housing_capacity{0};
  std::uint32_t employed{0};
  std::uint32_t unemployed{0};

  std::float_t satisfaction{0.5f};
  std::float_t loyalty{0.5f};

  std::float_t power_production{0.0f};
  std::float_t power_consumption{0.0f};

  std::float_t food_production{0.0f};
  std::float_t food_consumption{0.0f};
}; // struct town_stats

struct town_connection {
  town_id target_town{invalid_town_id};
  path_category connection_type{path_category::none};
  std::float_t distance{0.0f};
  std::float_t transport_capacity{0.0f};
}; // struct town_connection

struct town {
  town_id id{invalid_town_id};
  std::string name;

  // Grid position of town hall
  std::int32_t center_x{0};
  std::int32_t center_y{0};

  // Claim radius in chunks
  std::uint32_t claim_radius{3};

  // Chunks belonging to this town
  std::unordered_set<cell_coordinates, cell_coordinates_hash> claimed_chunks;

  // Buildings belonging to this town
  std::vector<std::uint16_t> building_ids;

  // Economy
  town_resources resources;
  town_stats stats;

  // Connections to other towns
  std::vector<town_connection> connections;

  // Is this town's data dirty and needs recalculation
  bool is_dirty{true};

}; // struct town

} // namespace demo

#endif // DEMO_TOWN_HPP_
