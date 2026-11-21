// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_NAV_PATH_CORRIDOR_HPP_
#define LIBSBX_PHYSICS_NAV_PATH_CORRIDOR_HPP_

#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include <libsbx/math/vector3.hpp>

#include <libsbx/physics/nav/navmesh_query.hpp>

namespace sbx::physics {

struct path_corridor {
  math::vector3 position{};
  math::vector3 target{};
  std::vector<poly_reference> path{};
}; // struct path_corridor

auto corridor_reset(path_corridor& corridor, poly_reference reference, const math::vector3& pos) -> void;

auto corridor_set_corridor(path_corridor& corridor, const math::vector3& target, std::span<const poly_reference> path) -> void;

[[nodiscard]] auto corridor_move_position(path_corridor& corridor, const navmesh& mesh, const math::vector3& new_pos) -> math::vector3;

auto corridor_find_corners(const path_corridor& corridor, const navmesh& mesh, std::size_t max_corners, std::vector<straight_path_point>& corners) -> void;

auto optimize_path_visibility(path_corridor& corridor, const navmesh& mesh, const math::vector3& next, std::float_t path_optimization_range) -> void;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_NAV_PATH_CORRIDOR_HPP_
