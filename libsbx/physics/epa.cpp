// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/epa.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include <libsbx/math/constants.hpp>

namespace sbx::physics {

namespace {

struct epa_face {
  std::array<std::uint32_t, 3> indices{};
  math::vector3 normal{math::vector3::up};
  std::float_t distance{0.0f}; // signed distance from the origin to this face's plane, along normal
}; // struct epa_face

// Builds one polytope face from three existing vertex indices, orienting its normal away from the
// origin (which the caller guarantees lies inside the polytope) so `distance` comes out >= 0.
auto add_face(const std::vector<support_point>& vertices, std::vector<epa_face>& faces, std::uint32_t i0, std::uint32_t i1, std::uint32_t i2) -> void {
  auto normal = math::vector3::cross(vertices[i1].point - vertices[i0].point, vertices[i2].point - vertices[i0].point);
  const auto length = normal.length();

  if (length <= math::epsilonf) {
    return; // degenerate (collinear) face -- skip, the polytope stays valid without it
  }

  normal = normal * (1.0f / length);

  auto distance = math::vector3::dot(normal, vertices[i0].point);

  if (distance < 0.0f) {
    normal = -normal;
    distance = -distance;
    std::swap(i1, i2);
  }

  faces.push_back(epa_face{{i0, i1, i2}, normal, distance});
}

// Reconstructs world-space witness points on A/B for the origin's projection onto `face`, by
// barycentric-blending the face's three vertices' own witness points (Ericson, "Real-Time
// Collision Detection" 3.4).
auto reconstruct_result(const std::vector<support_point>& vertices, const epa_face& face) -> epa_result {
  const auto& v0 = vertices[face.indices[0]];
  const auto& v1 = vertices[face.indices[1]];
  const auto& v2 = vertices[face.indices[2]];

  const auto projected = face.normal * face.distance;

  const auto e01 = v1.point - v0.point;
  const auto e02 = v2.point - v0.point;
  const auto e0p = projected - v0.point;

  const auto d00 = math::vector3::dot(e01, e01);
  const auto d01 = math::vector3::dot(e01, e02);
  const auto d11 = math::vector3::dot(e02, e02);
  const auto d20 = math::vector3::dot(e0p, e01);
  const auto d21 = math::vector3::dot(e0p, e02);

  const auto denominator = d00 * d11 - d01 * d01;

  auto u = 1.0f;
  auto v = 0.0f;
  auto w = 0.0f;

  if (std::abs(denominator) > math::epsilonf) {
    v = (d11 * d20 - d01 * d21) / denominator;
    w = (d00 * d21 - d01 * d20) / denominator;
    u = 1.0f - v - w;
  }

  return epa_result{
    true,
    face.normal,
    face.distance,
    v0.point_on_a * u + v1.point_on_a * v + v2.point_on_a * w,
    v0.point_on_b * u + v1.point_on_b * v + v2.point_on_b * w
  };
}

} // namespace

auto epa_penetration(
  const convex_shape& a, const transform& pose_a,
  const convex_shape& b, const transform& pose_b,
  containers::static_vector<support_point, gjk_max_simplex_points> gjk_simplex
) -> epa_result {
  if (gjk_simplex.size() != gjk_max_simplex_points) {
    return epa_result{};
  }

  auto vertices = std::vector<support_point>{gjk_simplex.begin(), gjk_simplex.end()};
  auto faces = std::vector<epa_face>{};
  faces.reserve(16u);

  add_face(vertices, faces, 0u, 1u, 2u);
  add_face(vertices, faces, 0u, 3u, 1u);
  add_face(vertices, faces, 0u, 2u, 3u);
  add_face(vertices, faces, 1u, 3u, 2u);

  constexpr auto max_iterations = std::uint32_t{32};
  constexpr auto tolerance = 1e-4f;

  for (auto iteration = std::uint32_t{0}; iteration < max_iterations; ++iteration) {
    if (faces.empty()) {
      return epa_result{};
    }

    auto closest_index = std::size_t{0};

    for (auto index = std::size_t{1}; index < faces.size(); ++index) {
      if (faces[index].distance < faces[closest_index].distance) {
        closest_index = index;
      }
    }

    const auto closest = faces[closest_index];

    const auto support = minkowski_support(a, pose_a, b, pose_b, closest.normal);
    const auto support_distance = math::vector3::dot(support.point, closest.normal);

    if (support_distance - closest.distance < tolerance) {
      return reconstruct_result(vertices, closest);
    }

    const auto new_index = static_cast<std::uint32_t>(vertices.size());
    vertices.push_back(support);

    // Remove every face visible from the new point, recording the horizon: edges owned by exactly
    // one removed face (an edge shared by two removed faces cancels out).
    auto horizon = std::vector<std::pair<std::uint32_t, std::uint32_t>>{};

    const auto toggle_edge = [&horizon](std::uint32_t i0, std::uint32_t i1) {
      const auto reverse = std::ranges::find(horizon, std::pair{i1, i0});

      if (reverse != horizon.end()) {
        horizon.erase(reverse);
      } else {
        horizon.emplace_back(i0, i1);
      }
    };

    for (auto it = faces.begin(); it != faces.end(); ) {
      if (math::vector3::dot(it->normal, support.point - vertices[it->indices[0]].point) > 0.0f) {
        toggle_edge(it->indices[0], it->indices[1]);
        toggle_edge(it->indices[1], it->indices[2]);
        toggle_edge(it->indices[2], it->indices[0]);

        it = faces.erase(it);
      } else {
        ++it;
      }
    }

    for (const auto& [i0, i1] : horizon) {
      add_face(vertices, faces, i0, i1, new_index);
    }
  }

  // Iteration budget exhausted without tight convergence -- return the best (closest) face found,
  // an acceptable approximation since GJK already confirmed true overlap.
  if (faces.empty()) {
    return epa_result{};
  }

  auto closest_index = std::size_t{0};

  for (auto index = std::size_t{1}; index < faces.size(); ++index) {
    if (faces[index].distance < faces[closest_index].distance) {
      closest_index = index;
    }
  }

  return reconstruct_result(vertices, faces[closest_index]);
}

} // namespace sbx::physics
