// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/convex_hull_cache.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

#include <libsbx/physics/quickhull.hpp>

namespace sbx::physics {

// Picks up to max_count well-spread points from `points`: seeded with the 6 axis-extremal points,
// then greedily adding whichever remaining point maximizes its minimum distance to everything
// already picked (farthest-point sampling), until reaching the cap or running out of input.
// Only ever used as the rare-mesh fallback in _build below, over the true hull's own vertices --
// not over the raw mesh cloud.
[[nodiscard]] auto farthest_point_sample(const std::vector<math::vector3>& points, std::size_t max_count) -> std::vector<math::vector3> {
  if (points.size() <= max_count) {
    return points;
  }

  auto selected = std::vector<std::size_t>{};
  selected.reserve(max_count);

  const auto try_add = [&](std::size_t index) {
    if (selected.size() >= max_count || std::ranges::find(selected, index) != selected.end()) {
      return;
    }

    selected.push_back(index);
  };

  for (auto axis = std::size_t{0}; axis < 3u; ++axis) {
    auto min_index = std::size_t{0};
    auto max_index = std::size_t{0};

    for (auto index = std::size_t{1}; index < points.size(); ++index) {
      if (points[index][axis] < points[min_index][axis]) {
        min_index = index;
      }

      if (points[index][axis] > points[max_index][axis]) {
        max_index = index;
      }
    }

    try_add(min_index);
    try_add(max_index);
  }

  while (selected.size() < max_count && selected.size() < points.size()) {
    auto best_index = std::optional<std::size_t>{};
    auto best_min_distance_squared = -1.0f;

    for (auto index = std::size_t{0}; index < points.size(); ++index) {
      if (std::ranges::find(selected, index) != selected.end()) {
        continue;
      }

      auto min_distance_squared = std::numeric_limits<std::float_t>::max();

      for (const auto selected_index : selected) {
        min_distance_squared = std::min(min_distance_squared, math::vector3::distance_squared(points[index], points[selected_index]));
      }

      if (min_distance_squared > best_min_distance_squared) {
        best_min_distance_squared = min_distance_squared;
        best_index = index;
      }
    }

    if (!best_index) {
      break;
    }

    selected.push_back(*best_index);
  }

  auto result = std::vector<math::vector3>{};
  result.reserve(selected.size());

  for (const auto index : selected) {
    result.push_back(points[index]);
  }

  return result;
}

auto convex_hull_cache::get_or_build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> const convex_hull_data& {
  if (!_cache.contains(mesh_id)) {
    _cache.emplace(mesh_id, _build(assets_module, mesh_id));
  }

  return _cache.at(mesh_id);
}

auto convex_hull_cache::clear() -> void {
  _cache.clear();
}

auto convex_hull_cache::_build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> convex_hull_data {
  auto data = convex_hull_data{};

  const auto cooked = assets_module.resolve_mesh_collision_data(mesh_id);

  if (!cooked || cooked->vertices.empty()) {
    return data;
  }

  auto positions = std::vector<math::vector3>{};
  positions.reserve(cooked->vertices.size());

  for (const auto& vertex : cooked->vertices) {
    positions.push_back(vertex.position);
  }

  auto hull = compute_convex_hull(positions);

  if (hull.vertices.size() > convex_hull_max_points) {
    // Rare: the mesh's exact hull already exceeds the budget (a very round or highly detailed
    // convex shape). Coarsen by farthest-point-sampling the *true* hull's own vertices -- every one
    // of these is already a genuine extremal point of the mesh, unlike sampling the raw mesh cloud
    // directly -- down to the cap, then re-hull that small, already-representative set.
    hull = compute_convex_hull(farthest_point_sample(hull.vertices, convex_hull_max_points));
  }

  for (const auto& vertex : hull.vertices) {
    data.points.push_back(vertex);
  }

  for (const auto& face : hull.faces) {
    data.faces.push_back(convex_hull_face{{
      static_cast<std::uint16_t>(face.indices[0]),
      static_cast<std::uint16_t>(face.indices[1]),
      static_cast<std::uint16_t>(face.indices[2])
    }});
  }

  data.local_bounds = math::volume::construct(data.points);

  return data;
}

} // namespace sbx::physics
