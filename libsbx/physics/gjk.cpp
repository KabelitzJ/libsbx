// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/gjk.hpp>

#include <cmath>
#include <cstdint>
#include <optional>

#include <libsbx/math/constants.hpp>

namespace sbx::physics {

auto support_world(const convex_shape& shape, const transform& pose, const math::vector3& world_direction) -> math::vector3 {
  // Exact for any diagonal scale S: support_{S*K}(d) = max_p dot(S ⊙ (rotation⁻¹ * d), p), so the search direction is scaled componentwise like the point.
  const auto local_direction = math::quaternion::conjugate(pose.rotation) * world_direction;
  const auto local_point = find_furthest_point(shape, local_direction * pose.scale);

  return pose.position + pose.rotation * (local_point * pose.scale);
}

auto minkowski_support(const convex_shape& a, const transform& pose_a, const convex_shape& b, const transform& pose_b, const math::vector3& world_direction) -> support_point {
  const auto point_on_a = support_world(a, pose_a, world_direction);
  const auto point_on_b = support_world(b, pose_b, -world_direction);

  return support_point{point_on_a - point_on_b, point_on_a, point_on_b};
}

using simplex_type = containers::static_vector<support_point, gjk_max_simplex_points>;

[[nodiscard]] auto same_direction(const math::vector3& a, const math::vector3& b) noexcept -> bool {
  return math::vector3::dot(a, b) > 0.0f;
}

auto do_line(simplex_type& simplex, math::vector3& direction) -> bool {
  const auto a = simplex[1];
  const auto b = simplex[0];

  const auto ab = b.point - a.point;
  const auto ao = -a.point;

  if (same_direction(ab, ao)) {
    direction = math::vector3::cross(math::vector3::cross(ab, ao), ab);
  } else {
    simplex = simplex_type{a};
    direction = ao;
  }

  return false;
}

auto do_triangle(simplex_type& simplex, math::vector3& direction) -> bool {
  const auto a = simplex[2];
  const auto b = simplex[1];
  const auto c = simplex[0];

  const auto ab = b.point - a.point;
  const auto ac = c.point - a.point;
  const auto ao = -a.point;
  const auto abc = math::vector3::cross(ab, ac);

  if (same_direction(math::vector3::cross(abc, ac), ao)) {
    if (same_direction(ac, ao)) {
      simplex = simplex_type{c, a};
      direction = math::vector3::cross(math::vector3::cross(ac, ao), ac);
    } else {
      simplex = simplex_type{b, a};
      return do_line(simplex, direction);
    }
  } else if (same_direction(math::vector3::cross(ab, abc), ao)) {
    simplex = simplex_type{b, a};
    return do_line(simplex, direction);
  } else if (same_direction(abc, ao)) {
    simplex = simplex_type{c, b, a};
    direction = abc;
  } else {
    simplex = simplex_type{b, c, a};
    direction = -abc;
  }

  return false;
}

auto do_tetrahedron(simplex_type& simplex, math::vector3& direction) -> bool {
  const auto a = simplex[3];
  const auto b = simplex[2];
  const auto c = simplex[1];
  const auto d = simplex[0];

  const auto ab = b.point - a.point;
  const auto ac = c.point - a.point;
  const auto ad = d.point - a.point;
  const auto ao = -a.point;

  const auto abc = math::vector3::cross(ab, ac);
  const auto acd = math::vector3::cross(ac, ad);
  const auto adb = math::vector3::cross(ad, ab);

  if (same_direction(abc, ao)) {
    simplex = simplex_type{c, b, a};
    return do_triangle(simplex, direction);
  }

  if (same_direction(acd, ao)) {
    simplex = simplex_type{d, c, a};
    return do_triangle(simplex, direction);
  }

  if (same_direction(adb, ao)) {
    simplex = simplex_type{b, d, a};
    return do_triangle(simplex, direction);
  }

  // Inside all three side faces: the tetrahedron encloses the origin.
  return true;
}

// The Voronoi-region test for the simplex's current size: shrinks it to the feature closest to the origin and points `direction` from it toward the origin. True once a tetrahedron encloses the origin.
auto do_simplex(simplex_type& simplex, math::vector3& direction) -> bool {
  switch (simplex.size()) {
    case 2: return do_line(simplex, direction);
    case 3: return do_triangle(simplex, direction);
    case 4: return do_tetrahedron(simplex, direction);
    default: return false;
  }
}

auto gjk_intersect(const convex_shape& a, const transform& pose_a, const convex_shape& b, const transform& pose_b) -> gjk_result {
  constexpr auto max_iterations = std::uint32_t{32};

  auto direction = pose_b.position - pose_a.position;

  if (direction.length_squared() < math::epsilonf) {
    direction = math::vector3::right;
  }

  auto simplex = simplex_type{};
  simplex.push_back(minkowski_support(a, pose_a, b, pose_b, direction));

  if (math::vector3::dot(simplex[0].point, direction) < 0.0f) {
    return gjk_result{false, {}};
  }

  direction = -simplex[0].point;

  for (auto iteration = std::uint32_t{0}; iteration < max_iterations; ++iteration) {
    const auto point = minkowski_support(a, pose_a, b, pose_b, direction);

    if (math::vector3::dot(point.point, direction) < 0.0f) {
      return gjk_result{false, {}};
    }

    simplex.push_back(point);

    if (do_simplex(simplex, direction)) {
      return gjk_result{true, simplex};
    }
  }

  return gjk_result{false, {}};
}

struct weighted_vertex {
  support_point vertex;
  std::float_t weight;
}; // struct weighted_vertex

using weighted_simplex = containers::static_vector<weighted_vertex, gjk_max_simplex_points>;

// The sub-simplex spanning the closest point to the origin, with that point's barycentric weights.
struct closest_feature {
  weighted_simplex simplex;
  math::vector3 point;
}; // struct closest_feature

[[nodiscard]] auto closest_on_segment(const support_point& a, const support_point& b) -> closest_feature {
  const auto ab = b.point - a.point;
  const auto length_squared = math::vector3::dot(ab, ab);
  const auto t = length_squared > math::epsilonf ? -math::vector3::dot(a.point, ab) / length_squared : 0.0f;

  if (t <= 0.0f) {
    return closest_feature{weighted_simplex{weighted_vertex{a, 1.0f}}, a.point};
  }

  if (t >= 1.0f) {
    return closest_feature{weighted_simplex{weighted_vertex{b, 1.0f}}, b.point};
  }

  return closest_feature{weighted_simplex{weighted_vertex{a, 1.0f - t}, weighted_vertex{b, t}}, a.point + ab * t};
}

// Ericson's closest-point-on-triangle (Real-Time Collision Detection 5.1.5) with the query point at the origin.
[[nodiscard]] auto closest_on_triangle(const support_point& a, const support_point& b, const support_point& c) -> closest_feature {
  const auto ab = b.point - a.point;
  const auto ac = c.point - a.point;

  const auto d1 = math::vector3::dot(ab, -a.point);
  const auto d2 = math::vector3::dot(ac, -a.point);

  if (d1 <= 0.0f && d2 <= 0.0f) {
    return closest_feature{weighted_simplex{weighted_vertex{a, 1.0f}}, a.point};
  }

  const auto d3 = math::vector3::dot(ab, -b.point);
  const auto d4 = math::vector3::dot(ac, -b.point);

  if (d3 >= 0.0f && d4 <= d3) {
    return closest_feature{weighted_simplex{weighted_vertex{b, 1.0f}}, b.point};
  }

  const auto vc = d1 * d4 - d3 * d2;

  if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
    const auto v = d1 / (d1 - d3);
    return closest_feature{weighted_simplex{weighted_vertex{a, 1.0f - v}, weighted_vertex{b, v}}, a.point + ab * v};
  }

  const auto d5 = math::vector3::dot(ab, -c.point);
  const auto d6 = math::vector3::dot(ac, -c.point);

  if (d6 >= 0.0f && d5 <= d6) {
    return closest_feature{weighted_simplex{weighted_vertex{c, 1.0f}}, c.point};
  }

  const auto vb = d5 * d2 - d1 * d6;

  if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
    const auto w = d2 / (d2 - d6);
    return closest_feature{weighted_simplex{weighted_vertex{a, 1.0f - w}, weighted_vertex{c, w}}, a.point + ac * w};
  }

  const auto va = d3 * d6 - d5 * d4;

  if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
    const auto w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return closest_feature{weighted_simplex{weighted_vertex{b, 1.0f - w}, weighted_vertex{c, w}}, b.point + (c.point - b.point) * w};
  }

  const auto denominator = 1.0f / (va + vb + vc);
  const auto v = vb * denominator;
  const auto w = vc * denominator;

  return closest_feature{weighted_simplex{weighted_vertex{a, 1.0f - v - w}, weighted_vertex{b, v}, weighted_vertex{c, w}}, a.point + ab * v + ac * w};
}

// Whether the origin and @p opposite lie on different sides of (or on) triangle abc's plane.
[[nodiscard]] auto origin_outside_face(const math::vector3& a, const math::vector3& b, const math::vector3& c, const math::vector3& opposite) -> bool {
  const auto normal = math::vector3::cross(b - a, c - a);
  return math::vector3::dot(-a, normal) * math::vector3::dot(opposite - a, normal) <= 0.0f;
}

// nullopt when the origin is inside the tetrahedron.
[[nodiscard]] auto closest_on_tetrahedron(const support_point& a, const support_point& b, const support_point& c, const support_point& d) -> std::optional<closest_feature> {
  auto best = std::optional<closest_feature>{};

  const auto consider_face = [&](const support_point& p, const support_point& q, const support_point& r, const support_point& opposite) {
    if (!origin_outside_face(p.point, q.point, r.point, opposite.point)) {
      return;
    }

    auto candidate = closest_on_triangle(p, q, r);

    if (!best || candidate.point.length_squared() < best->point.length_squared()) {
      best = std::move(candidate);
    }
  };

  consider_face(a, b, c, d);
  consider_face(a, c, d, b);
  consider_face(a, d, b, c);
  consider_face(b, d, c, a);

  return best;
}

auto gjk_distance(const convex_shape& a, const transform& pose_a, const convex_shape& b, const transform& pose_b) -> gjk_distance_result {
  constexpr auto max_iterations = std::uint32_t{64};
  // Within 1e-5 m counts as touching: below that, the closest-point direction is float noise.
  constexpr auto touching_distance_squared = 1e-10f;
  constexpr auto relative_tolerance = 1e-6f;

  auto initial_direction = pose_b.position - pose_a.position;

  if (initial_direction.length_squared() < math::epsilonf) {
    initial_direction = math::vector3::right;
  }

  const auto first = minkowski_support(a, pose_a, b, pose_b, initial_direction);

  auto simplex = weighted_simplex{weighted_vertex{first, 1.0f}};
  auto closest = first.point;

  for (auto iteration = std::uint32_t{0}; iteration < max_iterations; ++iteration) {
    const auto closest_length_squared = closest.length_squared();

    if (closest_length_squared <= touching_distance_squared) {
      return gjk_distance_result{true};
    }

    const auto candidate = minkowski_support(a, pose_a, b, pose_b, -closest);

    // No support point gets meaningfully closer to the origin than the current one: converged.
    if (closest_length_squared - math::vector3::dot(closest, candidate.point) <= relative_tolerance * closest_length_squared) {
      break;
    }

    auto feature = std::optional<closest_feature>{};

    switch (simplex.size()) {
      case 1: feature = closest_on_segment(simplex[0].vertex, candidate); break;
      case 2: feature = closest_on_triangle(simplex[0].vertex, simplex[1].vertex, candidate); break;
      default: feature = closest_on_tetrahedron(simplex[0].vertex, simplex[1].vertex, simplex[2].vertex, candidate); break;
    }

    if (!feature) {
      return gjk_distance_result{true};
    }

    // A stalled solve (degenerate simplex) can't improve further; keep the last good answer.
    if (feature->point.length_squared() >= closest_length_squared) {
      break;
    }

    simplex = feature->simplex;
    closest = feature->point;
  }

  auto result = gjk_distance_result{};

  for (const auto& [vertex, weight] : simplex) {
    result.point_on_a = result.point_on_a + vertex.point_on_a * weight;
    result.point_on_b = result.point_on_b + vertex.point_on_b * weight;
  }

  result.distance = closest.length();

  if (result.distance * result.distance <= touching_distance_squared) {
    return gjk_distance_result{true};
  }

  result.normal = closest / result.distance;

  return result;
}

} // namespace sbx::physics
