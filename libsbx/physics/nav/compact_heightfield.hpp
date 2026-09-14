// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_NAV_COMPACT_HEIGHTFIELD_HPP_
#define LIBSBX_PHYSICS_NAV_COMPACT_HEIGHTFIELD_HPP_

#include <cstdint>
#include <memory_resource>
#include <vector>

#include <libsbx/math/volume.hpp>

#include <libsbx/physics/nav/bake_arena.hpp>
#include <libsbx/physics/nav/heightfield.hpp>

namespace sbx::physics {

inline constexpr auto border_region = std::uint16_t{0x8000};

struct compact_heightfield {
  explicit compact_heightfield(std::pmr::memory_resource* resource = std::pmr::get_default_resource())
  : cells{resource}, spans{resource}, areas{resource} {}

  std::int32_t width{0};
  std::int32_t height{0};
  std::int32_t walkable_height{0};
  std::int32_t walkable_climb{0};
  std::int32_t border_size{0};
  math::volume bounds{};
  std::float_t cell_size{0.0f};
  std::float_t cell_height{0.0f};
  std::uint16_t max_regions{0};
  std::pmr::vector<compact_cell> cells;
  std::pmr::vector<compact_span> spans;
  std::pmr::vector<std::uint8_t> areas;
}; // struct compact_heightfield

[[nodiscard]] auto build_compact_heightfield(std::int32_t walkable_height, std::int32_t walkable_climb, const heightfield& hf, bake_arena& arena) -> compact_heightfield;

auto erode_walkable_area(std::int32_t erosion_radius, compact_heightfield& chf, bake_arena& arena) -> void;

[[nodiscard]] auto build_regions_monotone(compact_heightfield& chf, std::int32_t border_size, std::int32_t min_region_area, std::int32_t merge_region_area, bake_arena& arena) -> bool;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_NAV_COMPACT_HEIGHTFIELD_HPP_
