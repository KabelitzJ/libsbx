// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/quickhull.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

#include <libsbx/math/constants.hpp>

namespace sbx::physics {

// A face mid-construction: besides the triangle itself, tracks which of the not-yet-consumed input
// points lie outside it (candidates for expanding the hull further in this direction) -- the same
// "outside set" bookkeeping the Quickhull algorithm is named for.
struct working_face {
  std::array<std::uint32_t, 3> indices;
  math::vector3 normal; // unit, outward
  std::float_t plane_offset{0.0f}; // plane equation: dot(normal, x) == plane_offset
  std::vector<math::vector3> outside_points;
}; // struct working_face

inline constexpr auto visibility_epsilon = 1e-5f;

// Builds one face from three existing vertex indices, orienting its normal away from `reference`
// (a point already established to be strictly inside the hull-in-progress). Every face this
// algorithm ever creates -- the initial tetrahedron's 4 faces and every later horizon-expansion face
// -- uses the *same* reference point: the initial tetrahedron's own centroid. That stays valid
// throughout because the hull only ever grows outward from there, exactly the same reasoning
// epa.cpp uses with the origin (which EPA's polytope is guaranteed to always contain).
[[nodiscard]] auto make_face(const std::vector<math::vector3>& vertices, const math::vector3& reference, std::uint32_t a, std::uint32_t b, std::uint32_t c) -> working_face {
  auto normal = math::vector3::cross(vertices[b] - vertices[a], vertices[c] - vertices[a]);
  const auto length = normal.length();

  if (length > math::epsilonf) {
    normal = normal * (1.0f / length);
  }

  auto offset = math::vector3::dot(normal, vertices[a]);

  if (math::vector3::dot(normal, reference) > offset) {
    normal = -normal;
    offset = -offset;
    std::swap(b, c);
  }

  return working_face{{a, b, c}, normal, offset, {}};
}

// Redistributes `pool` among `faces` (only ever called with newly-created faces): each point goes
// to the first face it's outside of, or is dropped (now inside the larger hull) if it's outside none.
auto distribute(std::vector<math::vector3> pool, std::span<working_face> faces) -> void {
  for (auto& point : pool) {
    for (auto& face : faces) {
      if (math::vector3::dot(face.normal, point) > face.plane_offset + visibility_epsilon) {
        face.outside_points.push_back(std::move(point));
        break;
      }
    }
  }
}

[[nodiscard]] auto farthest_pair(std::span<const math::vector3> points) -> std::pair<std::uint32_t, std::uint32_t> {
  // The 6 axis extremes are a cheap, standard way to find a good (not necessarily optimal, but
  // reliably non-degenerate whenever the point set itself isn't) starting pair for the initial
  // simplex, without an O(n^2) all-pairs scan over the whole input.
  auto extremes = std::array<std::uint32_t, 6>{};

  for (auto axis = std::size_t{0}; axis < 3u; ++axis) {
    auto min_index = std::uint32_t{0};
    auto max_index = std::uint32_t{0};

    for (auto index = std::uint32_t{1}; index < points.size(); ++index) {
      if (points[index][axis] < points[min_index][axis]) {
        min_index = index;
      }

      if (points[index][axis] > points[max_index][axis]) {
        max_index = index;
      }
    }

    extremes[axis * 2u] = min_index;
    extremes[axis * 2u + 1u] = max_index;
  }

  auto best_a = extremes[0];
  auto best_b = extremes[1];
  auto best_distance_squared = -1.0f;

  for (auto i = std::size_t{0}; i < extremes.size(); ++i) {
    for (auto j = i + 1u; j < extremes.size(); ++j) {
      const auto distance_squared = math::vector3::distance_squared(points[extremes[i]], points[extremes[j]]);

      if (distance_squared > best_distance_squared) {
        best_distance_squared = distance_squared;
        best_a = extremes[i];
        best_b = extremes[j];
      }
    }
  }

  return {best_a, best_b};
}

auto compute_convex_hull(std::span<const math::vector3> points) -> hull_result {
  if (points.size() < 4u) {
    return hull_result{std::vector<math::vector3>{points.begin(), points.end()}, {}};
  }

  const auto [p0, p1] = farthest_pair(points);

  if (math::vector3::distance_squared(points[p0], points[p1]) <= math::epsilonf) {
    return hull_result{{points[0]}, {}}; // every point coincides
  }

  // p2: furthest from the line p0-p1.
  auto p2 = std::uint32_t{0};

  {
    const auto line_direction = math::vector3::normalized(points[p1] - points[p0]);
    auto best_distance_squared = -1.0f;

    for (auto index = std::uint32_t{0}; index < points.size(); ++index) {
      const auto offset = points[index] - points[p0];
      const auto projected = line_direction * math::vector3::dot(offset, line_direction);
      const auto distance_squared = (offset - projected).length_squared();

      if (distance_squared > best_distance_squared) {
        best_distance_squared = distance_squared;
        p2 = index;
      }
    }

    if (best_distance_squared <= math::epsilonf) {
      return hull_result{{points[p0], points[p1]}, {}}; // every point is collinear
    }
  }

  // p3: furthest (by absolute distance, either side) from the plane p0/p1/p2.
  auto p3 = std::uint32_t{0};

  {
    auto plane_normal = math::vector3::cross(points[p1] - points[p0], points[p2] - points[p0]);
    plane_normal = math::vector3::normalized(plane_normal);
    const auto plane_offset = math::vector3::dot(plane_normal, points[p0]);

    auto best_distance = -1.0f;

    for (auto index = std::uint32_t{0}; index < points.size(); ++index) {
      const auto distance = std::abs(math::vector3::dot(plane_normal, points[index]) - plane_offset);

      if (distance > best_distance) {
        best_distance = distance;
        p3 = index;
      }
    }

    if (best_distance <= math::epsilonf) {
      return hull_result{std::vector<math::vector3>{points.begin(), points.end()}, {}}; // every point is coplanar
    }
  }

  auto vertices = std::vector<math::vector3>{points[p0], points[p1], points[p2], points[p3]};
  const auto centroid = (vertices[0] + vertices[1] + vertices[2] + vertices[3]) * 0.25f;

  auto faces = std::vector<working_face>{
    make_face(vertices, centroid, 0u, 1u, 2u),
    make_face(vertices, centroid, 0u, 3u, 1u),
    make_face(vertices, centroid, 0u, 2u, 3u),
    make_face(vertices, centroid, 1u, 3u, 2u)
  };

  {
    auto initial_pool = std::vector<math::vector3>{points.begin(), points.end()};
    distribute(std::move(initial_pool), faces);
  }

  auto progress = true;

  while (progress) {
    progress = false;

    for (auto face_index = std::size_t{0}; face_index < faces.size(); ++face_index) {
      if (faces[face_index].outside_points.empty()) {
        continue;
      }

      progress = true;

      // The furthest outside point for this face becomes the next hull vertex ("apex").
      auto apex_local_index = std::size_t{0};
      auto apex_distance = -std::numeric_limits<std::float_t>::max();

      for (auto index = std::size_t{0}; index < faces[face_index].outside_points.size(); ++index) {
        const auto distance = math::vector3::dot(faces[face_index].normal, faces[face_index].outside_points[index]) - faces[face_index].plane_offset;

        if (distance > apex_distance) {
          apex_distance = distance;
          apex_local_index = index;
        }
      }

      const auto apex = faces[face_index].outside_points[apex_local_index];
      const auto apex_index = static_cast<std::uint32_t>(vertices.size());
      vertices.push_back(apex);

      // Remove every face visible from apex, collecting their outside points into one pool and
      // their boundary into the horizon (an edge shared by two removed faces cancels out -- the
      // same toggle technique epa.cpp uses to find its horizon).
      auto pool = std::vector<math::vector3>{};
      auto horizon = std::vector<std::pair<std::uint32_t, std::uint32_t>>{};

      const auto toggle_edge = [&horizon](std::uint32_t a, std::uint32_t b) {
        const auto reverse = std::ranges::find(horizon, std::pair{b, a});

        if (reverse != horizon.end()) {
          horizon.erase(reverse);
        } else {
          horizon.emplace_back(a, b);
        }
      };

      for (auto entry = faces.begin(); entry != faces.end(); ) {
        if (math::vector3::dot(entry->normal, apex - vertices[entry->indices[0]]) > visibility_epsilon) {
          for (auto& point : entry->outside_points) {
            pool.push_back(std::move(point));
          }

          toggle_edge(entry->indices[0], entry->indices[1]);
          toggle_edge(entry->indices[1], entry->indices[2]);
          toggle_edge(entry->indices[2], entry->indices[0]);

          entry = faces.erase(entry);
        } else {
          ++entry;
        }
      }

      auto new_faces = std::vector<working_face>{};
      new_faces.reserve(horizon.size());

      for (const auto& [a, b] : horizon) {
        new_faces.push_back(make_face(vertices, centroid, a, b, apex_index));
      }

      distribute(std::move(pool), new_faces);

      for (auto& new_face : new_faces) {
        faces.push_back(std::move(new_face));
      }

      break; // faces was resized -- restart the scan rather than continue indexing into it
    }
  }

  auto result = hull_result{};
  result.vertices = std::move(vertices);
  result.faces.reserve(faces.size());

  for (const auto& face : faces) {
    result.faces.push_back(hull_face{face.indices});
  }

  return result;
}

} // namespace sbx::physics
