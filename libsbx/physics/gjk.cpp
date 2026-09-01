// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/gjk.hpp>

#include <cstdint>

#include <libsbx/math/constants.hpp>

namespace sbx::physics {

auto support_world(const convex_shape& shape, const transform& pose, const math::vector3& world_direction) -> math::vector3 {
  const auto local_direction = math::quaternion::conjugate(pose.rotation) * world_direction;
  const auto local_point = find_furthest_point(shape, local_direction);

  return pose.position + pose.rotation * local_point;
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

  // Origin is on the inside of all three side faces (and abc/acd/adb all point toward it) -- it's
  // enclosed by the tetrahedron.
  return true;
}

// Dispatches on the simplex's current point count to the Voronoi-region test for that dimension.
// Each case may shrink the simplex to a lower-dimensional feature (discarding the point(s) not on
// the closest feature to the origin) and always leaves `direction` pointing from that feature
// toward the origin. Returns true only once a tetrahedron is found to enclose the origin.
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

} // namespace sbx::physics
