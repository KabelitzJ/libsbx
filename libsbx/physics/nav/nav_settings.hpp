// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_NAV_NAV_SETTINGS_HPP_
#define LIBSBX_PHYSICS_NAV_NAV_SETTINGS_HPP_

#include <cstdint>

#include <libsbx/physics/nav/heightfield.hpp>

namespace sbx::physics {

struct nav_settings {
  std::float_t agent_radius{0.3f};
  std::float_t agent_height{1.8f};
  std::float_t agent_max_slope{45.0f};
  std::float_t agent_max_climb{0.4f};
  std::float_t cell_size{0.2f};
  std::float_t cell_height{0.2f};
  std::float_t region_min_size{8.0f};
  std::float_t region_merge_size{20.0f};
  std::float_t edge_max_length{12.0f};
  std::float_t edge_max_error{1.3f};
  std::int32_t verts_per_poly{6};
}; // struct nav_settings

[[nodiscard]] auto to_config(const nav_settings& settings) -> config;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_NAV_NAV_SETTINGS_HPP_
