// SPDX-License-Identifier: MIT
#ifndef DEMO_TOWN_MANAGER_HPP_
#define DEMO_TOWN_MANAGER_HPP_

#include <unordered_map>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>

#include <demo/terrain/chunk.hpp>
#include <demo/terrain/grid.hpp>
#include <demo/terrain/terrain_module.hpp>

#include <demo/building/town.hpp>

namespace demo {

class town_manager {

public:

  town_manager() = default;

  auto found_town(const std::string& name, std::int32_t center_x, std::int32_t center_y, std::uint32_t claim_radius = 3) -> town_id {
    auto id = _next_id++;

    auto& new_town = _towns[id];
    new_town.id = id;
    new_town.name = name;
    new_town.center_x = center_x;
    new_town.center_y = center_y;
    new_town.claim_radius = claim_radius;

    // Claim chunks around the town center
    auto center_chunk = chunk_coord{
      static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(center_x) / chunk_size)),
      static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(center_y) / chunk_size))
    };

    auto radius = static_cast<std::int32_t>(claim_radius);

    for (auto chunk_y = center_chunk.y - radius; chunk_y <= center_chunk.y + radius; ++chunk_y) {
      for (auto chunk_x = center_chunk.x - radius; chunk_x <= center_chunk.x + radius; ++chunk_x) {
        auto distance_x = chunk_x - center_chunk.x;
        auto distance_y = chunk_y - center_chunk.y;
        auto distance_squared = distance_x * distance_x + distance_y * distance_y;

        // Circular claim area
        if (distance_squared <= radius * radius) {
          auto chunk_coordinates = chunk_coord{chunk_x, chunk_y};

          // Check if already claimed by another town
          if (_chunk_ownership.contains(chunk_coordinates)) {
            continue;
          }

          new_town.claimed_chunks.insert(chunk_coordinates);
          _chunk_ownership[chunk_coordinates] = id;
        }
      }
    }

    new_town.is_dirty = true;

    sbx::utility::logger<"town">::info("Founded town '{}' (id: {}) at cell ({}, {}) claiming {} chunks", name, id, center_x, center_y, new_town.claimed_chunks.size());

    return id;
  }

  auto get_town(town_id id) -> town* {
    auto entry = _towns.find(id);
    return (entry != _towns.end()) ? &entry->second : nullptr;
  }

  auto get_town(town_id id) const -> const town* {
    auto entry = _towns.find(id);
    return (entry != _towns.end()) ? &entry->second : nullptr;
  }

  auto get_town_at_cell(std::int32_t cell_x, std::int32_t cell_y) const -> town_id {
    auto chunk_coordinates = chunk_coord{
      static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(cell_x) / chunk_size)),
      static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(cell_y) / chunk_size))
    };

    auto entry = _chunk_ownership.find(chunk_coordinates);

    if (entry == _chunk_ownership.end()) {
      return invalid_town_id;
    }

    return entry->second;
  }

  auto get_town_at_chunk(chunk_coord chunk_coordinates) const -> town_id {
    auto entry = _chunk_ownership.find(chunk_coordinates);

    if (entry == _chunk_ownership.end()) {
      return invalid_town_id;
    }

    return entry->second;
  }

  auto is_chunk_claimed(chunk_coord chunk_coordinates) const -> bool {
    return _chunk_ownership.contains(chunk_coordinates);
  }

  auto can_found_town_at(std::int32_t center_x, std::int32_t center_y, std::uint32_t claim_radius = 3) const -> bool {
    auto center_chunk = chunk_coord{
      static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(center_x) / chunk_size)),
      static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(center_y) / chunk_size))
    };

    // At least the center chunk must be unclaimed
    if (_chunk_ownership.contains(center_chunk)) {
      return false;
    }

    // Check minimum distance to existing towns
    for (const auto& [existing_id, existing_town] : _towns) {
      auto distance_x = center_x - existing_town.center_x;
      auto distance_y = center_y - existing_town.center_y;
      auto distance = std::sqrt(static_cast<std::float_t>(distance_x * distance_x + distance_y * distance_y));

      // Minimum distance: sum of both claim radii in cells
      auto minimum_distance = static_cast<std::float_t>(claim_radius + existing_town.claim_radius) * chunk_size;

      if (distance < minimum_distance) {
        return false;
      }
    }

    return true;
  }

  auto expand_town(town_id id, std::uint32_t additional_radius = 1) -> std::uint32_t {
    auto* target_town = get_town(id);

    if (!target_town) {
      return 0;
    }

    auto center_chunk = chunk_coord{
      static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(target_town->center_x) / chunk_size)),
      static_cast<std::int32_t>(std::floor(static_cast<std::float_t>(target_town->center_y) / chunk_size))
    };

    target_town->claim_radius += additional_radius;

    auto new_radius = static_cast<std::int32_t>(target_town->claim_radius);
    auto chunks_added = 0u;

    for (auto chunk_y = center_chunk.y - new_radius; chunk_y <= center_chunk.y + new_radius; ++chunk_y) {
      for (auto chunk_x = center_chunk.x - new_radius; chunk_x <= center_chunk.x + new_radius; ++chunk_x) {
        auto distance_x = chunk_x - center_chunk.x;
        auto distance_y = chunk_y - center_chunk.y;
        auto distance_squared = distance_x * distance_x + distance_y * distance_y;

        if (distance_squared > new_radius * new_radius) {
          continue;
        }

        auto chunk_coordinates = chunk_coord{chunk_x, chunk_y};

        if (_chunk_ownership.contains(chunk_coordinates)) {
          continue;
        }

        target_town->claimed_chunks.insert(chunk_coordinates);
        _chunk_ownership[chunk_coordinates] = id;
        ++chunks_added;
      }
    }

    if (chunks_added > 0) {
      target_town->is_dirty = true;
      sbx::utility::logger<"town">::info("Expanded town '{}' by {} chunks (radius now {})", target_town->name, chunks_added, target_town->claim_radius);
    }

    return chunks_added;
  }

  auto connect_towns(town_id town_a, town_id town_b, path_category connection_type) -> bool {
    auto* first_town = get_town(town_a);
    auto* second_town = get_town(town_b);

    if (!first_town || !second_town) {
      return false;
    }

    // Check if already connected
    for (const auto& existing_connection : first_town->connections) {
      if (existing_connection.target_town == town_b) {
        return false;
      }
    }

    auto distance_x = static_cast<std::float_t>(first_town->center_x - second_town->center_x);
    auto distance_y = static_cast<std::float_t>(first_town->center_y - second_town->center_y);
    auto distance = std::sqrt(distance_x * distance_x + distance_y * distance_y) * grid::cell_size;

    auto transport_capacity = 0.0f;

    if (connection_type == path_category::road) {
      transport_capacity = 100.0f;
    } else if (connection_type == path_category::rail) {
      transport_capacity = 500.0f;
    }

    first_town->connections.push_back(town_connection{
      .target_town = town_b,
      .connection_type = connection_type,
      .distance = distance,
      .transport_capacity = transport_capacity,
    });

    second_town->connections.push_back(town_connection{
      .target_town = town_a,
      .connection_type = connection_type,
      .distance = distance,
      .transport_capacity = transport_capacity,
    });

    first_town->is_dirty = true;
    second_town->is_dirty = true;

    sbx::utility::logger<"town">::info("Connected '{}' <-> '{}' via {} (distance: {:.0f}m)", first_town->name, second_town->name, connection_type == path_category::road ? "road" : "rail", distance);

    return true;
  }

  auto town_count() const -> std::size_t {
    return _towns.size();
  }

  template<typename Callable>
  auto for_each_town(Callable&& callable) -> void {
    for (auto& [id, current_town] : _towns) {
      std::invoke(callable, id, current_town);
    }
  }

  template<typename Callable>
  auto for_each_town(Callable&& callable) const -> void {
    for (const auto& [id, current_town] : _towns) {
      std::invoke(callable, id, current_town);
    }
  }

  auto get_all_town_ids() const -> std::vector<town_id> {
    auto ids = std::vector<town_id>{};
    ids.reserve(_towns.size());

    for (const auto& [id, current_town] : _towns) {
      ids.push_back(id);
    }

    return ids;
  }

private:

  town_id _next_id{1};
  std::unordered_map<town_id, town> _towns;
  std::unordered_map<chunk_coord, town_id, chunk_coord_hash> _chunk_ownership;

}; // class town_manager

} // namespace demo

#endif // DEMO_TOWN_MANAGER_HPP_
