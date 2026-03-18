// SPDX-License-Identifier: MIT
#ifndef DEMO_TERRAIN_TERRAIN_INTEROP_HPP_
#define DEMO_TERRAIN_TERRAIN_INTEROP_HPP_

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/core/engine.hpp>

#include <demo/terrain/terrain_module.hpp>

namespace demo::terrain_interop {

static auto terrain_get_height_at(sbx::math::vector2* coord, std::float_t* height) -> void {
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  *height = terrain_module.get_height_at(coord->x(), coord->y());
}

static auto terrain_get_world_size(sbx::math::vector2* size) -> void {
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  auto width = static_cast<std::float_t>(terrain_module.world_width()) * terrain_module.cell_size();
  auto height = static_cast<std::float_t>(terrain_module.world_height()) * terrain_module.cell_size();

  *size = sbx::math::vector2{width, height};
}

static auto terrain_get_cell_size(std::float_t* cell_size) -> void {
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  *cell_size = terrain_module.cell_size();
}

static auto terrain_get_slope_at(sbx::math::vector2* cell, std::float_t* slope) -> void {
  auto& terrain_module = sbx::core::engine::get_module<demo::terrain_module>();

  *slope = terrain_module.get_slope_at(static_cast<std::int32_t>(cell->x()), static_cast<std::int32_t>(cell->y()));
}

static auto terrain_get_sea_level(std::float_t* sea_level) -> void {
  *sea_level = terrain_constants::sea_level;
}

} // namespace demo::terrain_interop

#endif // DEMO_TERRAIN_TERRAIN_INTEROP_HPP_
