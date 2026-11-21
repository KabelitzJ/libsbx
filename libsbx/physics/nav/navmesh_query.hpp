// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_NAV_NAVMESH_QUERY_HPP_
#define LIBSBX_PHYSICS_NAV_NAVMESH_QUERY_HPP_

#include <cstddef>
#include <span>
#include <vector>

#include <libsbx/math/vector3.hpp>

#include <libsbx/physics/nav/navmesh.hpp>

namespace sbx::physics {

struct path_result {
  std::vector<poly_reference> polys{};
  bool success{false};
}; // struct path_result

[[nodiscard]] auto find_path(const navmesh& mesh, poly_reference start, poly_reference end, const math::vector3& start_pos, const math::vector3& end_pos) -> path_result;

struct straight_path_point {
  math::vector3 position{};
  poly_reference reference{null_poly_reference};
}; // struct straight_path_point

auto find_straight_path(const navmesh& mesh, const math::vector3& start_pos, const math::vector3& end_pos, std::span<const poly_reference> poly_path, std::size_t max_points, std::vector<straight_path_point>& result) -> void;

struct surface_move_result {
  math::vector3 position{};
  std::vector<poly_reference> visited{};
}; // struct surface_move_result

[[nodiscard]] auto move_along_surface(const navmesh& mesh, poly_reference start_ref, const math::vector3& start_pos, const math::vector3& end_pos) -> surface_move_result;

struct raycast_result {
  std::float_t t{0.0f};
  std::vector<poly_reference> path{};
}; // struct raycast_result

[[nodiscard]] auto raycast(const navmesh& mesh, poly_reference start_ref, const math::vector3& start_pos, const math::vector3& end_pos) -> raycast_result;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_NAV_NAVMESH_QUERY_HPP_
