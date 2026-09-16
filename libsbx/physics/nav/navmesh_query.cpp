// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/nav/navmesh_query.hpp>

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace sbx::physics {

[[nodiscard]] auto distance_xz(const math::vector3& a, const math::vector3& b) -> std::float_t {
  const auto dx = a.x() - b.x();
  const auto dz = a.z() - b.z();

  return std::sqrt(dx * dx + dz * dz);
}

[[nodiscard]] auto find_path(const navmesh& mesh, poly_reference start, poly_reference end, const math::vector3& start_pos, const math::vector3& end_pos) -> path_result {
  if (start == null_poly_reference || end == null_poly_reference) {
    return path_result{};
  }

  if (start == end) {
    return path_result{std::vector<poly_reference>{start}, true};
  }

  struct open_entry {
    std::float_t f_score;
    poly_reference reference;
  };

  const auto compare = [](const open_entry& a, const open_entry& b) {
    return a.f_score > b.f_score;
  };

  auto open = std::priority_queue<open_entry, std::vector<open_entry>, decltype(compare)>{compare};

  auto g_score = std::unordered_map<poly_reference, std::float_t>{};
  auto came_from = std::unordered_map<poly_reference, poly_reference>{};
  auto node_position = std::unordered_map<poly_reference, math::vector3>{};
  auto closed = std::unordered_set<poly_reference>{};

  const auto heuristic = [&](const math::vector3& position) {
    return distance_xz(position, end_pos);
  };

  g_score[start] = 0.0f;
  node_position[start] = start_pos;
  open.push(open_entry{heuristic(start_pos), start});

  while (!open.empty()) {
    const auto current = open.top().reference;
    open.pop();

    if (current == end) {
      auto polys = std::vector<poly_reference>{current};
      auto entry = came_from.find(current);

      while (entry != came_from.end()) {
        polys.push_back(entry->second);
        entry = came_from.find(entry->second);
      }

      std::reverse(polys.begin(), polys.end());

      return path_result{std::move(polys), true};
    }

    if (closed.contains(current)) {
      continue;
    }

    closed.insert(current);

    const auto& poly = mesh.polys[poly_ref_to_index(current)];
    const auto current_pos = node_position[current];

    for (auto edge_index = std::size_t{0}; edge_index < poly.neighbors.size(); ++edge_index) {
      const auto neighbor = poly.neighbors[edge_index];

      if (neighbor == null_poly_reference || closed.contains(neighbor)) {
        continue;
      }

      const auto portal_point = poly_edge_midpoint(mesh, current, static_cast<std::uint32_t>(edge_index));
      const auto edge_cost = distance_xz(current_pos, portal_point);
      const auto tentative_g = g_score[current] + edge_cost;

      const auto existing = g_score.find(neighbor);

      if (existing == g_score.end() || tentative_g < existing->second) {
        g_score[neighbor] = tentative_g;
        came_from[neighbor] = current;
        node_position[neighbor] = portal_point;
        open.push(open_entry{tentative_g + heuristic(portal_point), neighbor});
      }
    }
  }

  return path_result{};
}

[[nodiscard]] auto triangle_area_2d(const math::vector3& a, const math::vector3& b, const math::vector3& c) -> std::float_t {
  const auto abx = b.x() - a.x();
  const auto abz = b.z() - a.z();
  const auto acx = c.x() - a.x();
  const auto acz = c.z() - a.z();

  return acx * abz - abx * acz;
}

[[nodiscard]] auto vertices_close(const math::vector3& a, const math::vector3& b) -> bool {
  constexpr auto threshold = 1.0f / (16384.0f * 16384.0f);

  return math::vector3::distance_squared(a, b) < threshold;
}

[[nodiscard]] auto append_straight_vertex(std::vector<straight_path_point>& result, const math::vector3& position, poly_reference reference, std::size_t max_points) -> bool {
  if (!result.empty() && vertices_close(result.back().position, position)) {
    result.back().reference = reference;

    return true;
  }

  result.push_back(straight_path_point{position, reference});

  return result.size() < max_points;
}

[[nodiscard]] auto find_straight_path(const navmesh& mesh, const math::vector3& start_pos, const math::vector3& end_pos, std::span<const poly_reference> poly_path, std::size_t max_points) -> std::vector<straight_path_point> {
  auto result = std::vector<straight_path_point>{};

  if (poly_path.empty() || max_points == 0) {
    return result;
  }

  const auto closest_start = closest_point_on_poly(mesh, poly_path[0], start_pos);
  const auto closest_end = closest_point_on_poly(mesh, poly_path.back(), end_pos);

  if (!append_straight_vertex(result, closest_start, poly_path[0], max_points)) {
    return result;
  }

  if (poly_path.size() == 1) {
    append_straight_vertex(result, closest_end, null_poly_reference, max_points);

    return result;
  }

  auto portal_apex = closest_start;
  auto portal_left = portal_apex;
  auto portal_right = portal_apex;

  auto apex_index = std::size_t{0};
  auto left_index = std::size_t{0};
  auto right_index = std::size_t{0};

  auto left_poly_ref = poly_path[0];
  auto right_poly_ref = poly_path[0];

  const auto path_size = poly_path.size();

  for (auto i = std::size_t{0}; i < path_size; ++i) {
    auto left = math::vector3{};
    auto right = math::vector3{};

    if (i + 1 < path_size) {
      if (!poly_portal_points(mesh, poly_path[i], poly_path[i + 1], left, right)) {
        break;
      }
    } else {
      left = closest_end;
      right = closest_end;
    }

    if (triangle_area_2d(portal_apex, portal_right, right) <= 0.0f) {
      if (vertices_close(portal_apex, portal_right) || triangle_area_2d(portal_apex, portal_left, right) > 0.0f) {
        portal_right = right;
        right_poly_ref = (i + 1 < path_size) ? poly_path[i + 1] : null_poly_reference;
        right_index = i;
      } else {
        portal_apex = portal_left;
        apex_index = left_index;

        if (!append_straight_vertex(result, portal_apex, left_poly_ref, max_points)) {
          return result;
        }

        portal_left = portal_apex;
        portal_right = portal_apex;
        left_index = apex_index;
        right_index = apex_index;

        i = apex_index;

        continue;
      }
    }

    if (triangle_area_2d(portal_apex, portal_left, left) >= 0.0f) {
      if (vertices_close(portal_apex, portal_left) || triangle_area_2d(portal_apex, portal_right, left) < 0.0f) {
        portal_left = left;
        left_poly_ref = (i + 1 < path_size) ? poly_path[i + 1] : null_poly_reference;
        left_index = i;
      } else {
        portal_apex = portal_right;
        apex_index = right_index;

        if (!append_straight_vertex(result, portal_apex, right_poly_ref, max_points)) {
          return result;
        }

        portal_left = portal_apex;
        portal_right = portal_apex;
        left_index = apex_index;
        right_index = apex_index;

        i = apex_index;

        continue;
      }
    }
  }

  append_straight_vertex(result, closest_end, null_poly_reference, max_points);

  return result;
}

[[nodiscard]] auto point_in_poly_xz(const navmesh& mesh, poly_reference reference, const math::vector3& point) -> bool {
  const auto& poly = mesh.polys[poly_ref_to_index(reference)];
  const auto count = poly.verts.size();

  auto inside = false;

  for (auto i = std::size_t{0}, j = count - 1; i < count; j = i++) {
    const auto& vi = mesh.verts[poly.verts[i]];
    const auto& vj = mesh.verts[poly.verts[j]];

    if ((vi.z() > point.z()) != (vj.z() > point.z()) && point.x() < (vj.x() - vi.x()) * (point.z() - vi.z()) / (vj.z() - vi.z()) + vi.x()) {
      inside = !inside;
    }
  }

  return inside;
}

[[nodiscard]] auto distance_point_segment_sqr_xz(const math::vector3& point, const math::vector3& a, const math::vector3& b, std::float_t& t) -> std::float_t {
  const auto abx = b.x() - a.x();
  const auto abz = b.z() - a.z();
  const auto apx = point.x() - a.x();
  const auto apz = point.z() - a.z();

  const auto ab_length_squared = abx * abx + abz * abz;

  t = ab_length_squared > 0.0f ? (apx * abx + apz * abz) / ab_length_squared : 0.0f;
  t = std::clamp(t, 0.0f, 1.0f);

  const auto closest_x = a.x() + abx * t;
  const auto closest_z = a.z() + abz * t;

  const auto dx = point.x() - closest_x;
  const auto dz = point.z() - closest_z;

  return dx * dx + dz * dz;
}

[[nodiscard]] auto move_along_surface(const navmesh& mesh, poly_reference start_ref, const math::vector3& start_pos, const math::vector3& end_pos) -> surface_move_result {
  auto result = surface_move_result{};

  if (start_ref == null_poly_reference) {
    result.position = start_pos;

    return result;
  }

  auto came_from = std::unordered_map<poly_reference, poly_reference>{};
  auto visited = std::unordered_set<poly_reference>{start_ref};
  auto queue = std::deque<poly_reference>{start_ref};

  const auto search_center = (start_pos + end_pos) * 0.5f;
  const auto half_distance = distance_xz(start_pos, end_pos) * 0.5f + 0.001f;
  const auto search_radius_sqr = half_distance * half_distance;

  auto best_ref = poly_reference{null_poly_reference};
  auto best_pos = start_pos;
  auto best_distance = std::numeric_limits<std::float_t>::max();

  while (!queue.empty()) {
    const auto current = queue.front();
    queue.pop_front();

    if (point_in_poly_xz(mesh, current, end_pos)) {
      best_ref = current;
      best_pos = end_pos;

      break;
    }

    const auto& poly = mesh.polys[poly_ref_to_index(current)];
    const auto count = poly.verts.size();

    for (auto i = std::size_t{0}; i < count; ++i) {
      const auto& a = mesh.verts[poly.verts[i]];
      const auto& b = mesh.verts[poly.verts[(i + 1u) % count]];
      const auto neighbor = poly.neighbors[i];

      if (neighbor == null_poly_reference) {
        auto t = 0.0f;
        const auto distance = distance_point_segment_sqr_xz(end_pos, a, b, t);

        if (distance < best_distance) {
          best_distance = distance;
          best_ref = current;
          best_pos = math::vector3{a.x() + (b.x() - a.x()) * t, a.y(), a.z() + (b.z() - a.z()) * t};
        }
      } else if (!visited.contains(neighbor)) {
        auto t = 0.0f;

        if (distance_point_segment_sqr_xz(search_center, a, b, t) <= search_radius_sqr) {
          visited.insert(neighbor);
          came_from[neighbor] = current;
          queue.push_back(neighbor);
        }
      }
    }
  }

  if (best_ref == null_poly_reference) {
    result.position = start_pos;
    result.visited.push_back(start_ref);

    return result;
  }

  result.position = best_pos;

  auto node = best_ref;
  result.visited.push_back(node);

  auto entry = came_from.find(node);

  while (entry != came_from.end()) {
    node = entry->second;
    result.visited.push_back(node);
    entry = came_from.find(node);
  }

  std::reverse(result.visited.begin(), result.visited.end());

  return result;
}

[[nodiscard]] auto intersect_segment_poly_xz(const math::vector3& p0, const math::vector3& p1, const navmesh& mesh, poly_reference ref, std::float_t& tmin, std::float_t& tmax, std::int32_t& seg_min, std::int32_t& seg_max) -> bool {
  const auto& poly = mesh.polys[poly_ref_to_index(ref)];
  const auto count = poly.verts.size();

  tmin = 0.0f;
  tmax = 1.0f;
  seg_min = -1;
  seg_max = -1;

  const auto dir_x = p1.x() - p0.x();
  const auto dir_z = p1.z() - p0.z();

  for (auto i = std::size_t{0}, j = count - 1; i < count; j = i++) {
    const auto& vi = mesh.verts[poly.verts[i]];
    const auto& vj = mesh.verts[poly.verts[j]];

    const auto edge_x = vi.x() - vj.x();
    const auto edge_z = vi.z() - vj.z();
    const auto diff_x = p0.x() - vj.x();
    const auto diff_z = p0.z() - vj.z();

    const auto n = edge_x * diff_z - edge_z * diff_x;
    const auto d = dir_x * edge_z - dir_z * edge_x;

    if (std::abs(d) < 1e-6f) {
      if (n < 0.0f) {
        return false;
      }

      continue;
    }

    const auto t = n / d;

    if (d < 0.0f) {
      if (t > tmin) {
        tmin = t;
        seg_min = static_cast<std::int32_t>(j);

        if (tmin > tmax) {
          return false;
        }
      }
    } else {
      if (t < tmax) {
        tmax = t;
        seg_max = static_cast<std::int32_t>(j);

        if (tmax < tmin) {
          return false;
        }
      }
    }
  }

  return true;
}

[[nodiscard]] auto raycast(const navmesh& mesh, poly_reference start_ref, const math::vector3& start_pos, const math::vector3& end_pos) -> raycast_result {
  auto result = raycast_result{};

  auto current = start_ref;

  while (current != null_poly_reference) {
    auto tmin = 0.0f;
    auto tmax = 1.0f;
    auto seg_min = std::int32_t{-1};
    auto seg_max = std::int32_t{-1};

    if (!intersect_segment_poly_xz(start_pos, end_pos, mesh, current, tmin, tmax, seg_min, seg_max)) {
      return result;
    }

    result.t = std::max(result.t, tmax);
    result.path.push_back(current);

    if (seg_max == -1) {
      result.t = std::numeric_limits<std::float_t>::max();

      return result;
    }

    const auto& poly = mesh.polys[poly_ref_to_index(current)];
    current = poly.neighbors[static_cast<std::size_t>(seg_max)];
  }

  return result;
}

} // namespace sbx::physics
