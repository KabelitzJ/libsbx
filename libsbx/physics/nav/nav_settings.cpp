// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/nav/nav_settings.hpp>

#include <cmath>

namespace sbx::physics {

[[nodiscard]] auto to_config(const nav_settings& settings) -> config {
  auto cfg = config{};

  cfg.tile_size = 0;
  cfg.border_size = 0;
  cfg.cell_size = settings.cell_size;
  cfg.cell_height = settings.cell_height;
  cfg.walkable_slope_angle = settings.agent_max_slope;
  cfg.walkable_height = static_cast<std::int32_t>(std::ceil(settings.agent_height / settings.cell_height));
  cfg.walkable_climb = static_cast<std::int32_t>(std::floor(settings.agent_max_climb / settings.cell_height));
  cfg.walkable_radius = static_cast<std::int32_t>(std::ceil(settings.agent_radius / settings.cell_size));
  cfg.max_edge_length = static_cast<std::int32_t>(settings.edge_max_length / settings.cell_size);
  cfg.max_simplification_error = settings.edge_max_error;
  cfg.min_region_area = static_cast<std::int32_t>(settings.region_min_size * settings.region_min_size);
  cfg.merge_region_area = static_cast<std::int32_t>(settings.region_merge_size * settings.region_merge_size);
  cfg.max_verts_per_poly = settings.verts_per_poly;
  cfg.detail_sample_distance = settings.cell_size * 6.0f;
  cfg.detail_sample_max_error = settings.cell_height;

  return cfg;
}

} // namespace sbx::physics
