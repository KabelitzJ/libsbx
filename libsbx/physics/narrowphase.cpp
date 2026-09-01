// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/narrowphase.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <variant>
#include <vector>

#include <libsbx/math/constants.hpp>

#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/epa.hpp>
#include <libsbx/physics/gjk.hpp>
#include <libsbx/physics/mesh_collision_cache.hpp>
#include <libsbx/physics/convex_hull_cache.hpp>

namespace sbx::physics {

// One narrowphase point before it's turned into a full contact_point (anchors/feature id are
// filled in by generate_contact, which is the only place that knows about the owning nodes).
struct narrow_point {
  math::vector3 point{math::vector3::zero};
  std::float_t depth{0.0f};
}; // struct narrow_point

struct narrow_result {
  math::vector3 normal{math::vector3::up}; // world space, A -> B
  containers::static_vector<narrow_point, max_manifold_points> points{};
}; // struct narrow_result

[[nodiscard]] auto compose_pose(const scenes::local_transform& local, const math::vector3& offset, const math::quaternion& rotation) -> transform {
  return transform{
    local.position + local.rotation * offset,
    math::quaternion::normalized(local.rotation * rotation)
  };
}

[[nodiscard]] auto closest_point_on_segment(const math::vector3& point, const math::vector3& a, const math::vector3& b) -> math::vector3 {
  const auto ab = b - a;
  const auto length_squared = ab.length_squared();

  if (length_squared <= math::epsilonf) {
    return a;
  }

  const auto t = std::clamp(math::vector3::dot(point - a, ab) / length_squared, 0.0f, 1.0f);

  return a + ab * t;
}

// Ericson, "Real-Time Collision Detection" 5.1.9.
auto closest_points_segment_segment(const math::vector3& p1, const math::vector3& q1, const math::vector3& p2, const math::vector3& q2, math::vector3& c1, math::vector3& c2) -> void {
  const auto d1 = q1 - p1;
  const auto d2 = q2 - p2;
  const auto r = p1 - p2;

  const auto a = math::vector3::dot(d1, d1);
  const auto e = math::vector3::dot(d2, d2);
  const auto f = math::vector3::dot(d2, r);

  auto s = 0.0f;
  auto t = 0.0f;

  if (a <= math::epsilonf && e <= math::epsilonf) {
    c1 = p1;
    c2 = p2;
    return;
  }

  if (a <= math::epsilonf) {
    s = 0.0f;
    t = std::clamp(f / e, 0.0f, 1.0f);
  } else {
    const auto c = math::vector3::dot(d1, r);

    if (e <= math::epsilonf) {
      t = 0.0f;
      s = std::clamp(-c / a, 0.0f, 1.0f);
    } else {
      const auto b = math::vector3::dot(d1, d2);
      const auto denominator = a * e - b * b;

      s = (std::abs(denominator) > math::epsilonf) ? std::clamp((b * f - c * e) / denominator, 0.0f, 1.0f) : 0.0f;
      t = (b * s + f) / e;

      if (t < 0.0f) {
        t = 0.0f;
        s = std::clamp(-c / a, 0.0f, 1.0f);
      } else if (t > 1.0f) {
        t = 1.0f;
        s = std::clamp((b - c) / a, 0.0f, 1.0f);
      }
    }
  }

  c1 = p1 + d1 * s;
  c2 = p2 + d2 * t;
}

[[nodiscard]] auto single_point(const math::vector3& normal, const math::vector3& point, std::float_t depth) -> narrow_result {
  auto result = narrow_result{normal, {}};
  result.points.push_back(narrow_point{point, depth});
  return result;
}

// -- Closed forms -----------------------------------------------------------------------------

[[nodiscard]] auto sphere_sphere(const math::vector3& center_a, std::float_t radius_a, const math::vector3& center_b, std::float_t radius_b) -> std::optional<narrow_result> {
  const auto delta = center_b - center_a;
  const auto radius_sum = radius_a + radius_b;
  const auto distance_squared = delta.length_squared();

  if (distance_squared >= radius_sum * radius_sum) {
    return std::nullopt;
  }

  const auto distance = std::sqrt(distance_squared);
  const auto normal = (distance > math::epsilonf) ? math::vector3{delta * (1.0f / distance)} : math::vector3::up;
  const auto depth = radius_sum - distance;

  const auto point_on_a = center_a + normal * radius_a;
  const auto point_on_b = center_b - normal * radius_b;

  return single_point(normal, (point_on_a + point_on_b) * 0.5f, depth);
}

// Returns a normal pointing from the box toward the sphere (box acts as the reference shape).
[[nodiscard]] auto sphere_box(const math::vector3& sphere_center, std::float_t sphere_radius, const math::vector3& box_center, const math::quaternion& box_rotation, const math::vector3& half_extents) -> std::optional<narrow_result> {
  const auto local = math::quaternion::conjugate(box_rotation) * (sphere_center - box_center);

  const auto inside = std::abs(local.x()) <= half_extents.x() && std::abs(local.y()) <= half_extents.y() && std::abs(local.z()) <= half_extents.z();

  auto normal_local = math::vector3::zero;
  auto point_local = local;
  auto depth = 0.0f;

  if (inside) {
    const auto dx = half_extents.x() - std::abs(local.x());
    const auto dy = half_extents.y() - std::abs(local.y());
    const auto dz = half_extents.z() - std::abs(local.z());

    if (dx <= dy && dx <= dz) {
      normal_local = math::vector3{(local.x() >= 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f};
      point_local.x() = (local.x() >= 0.0f) ? half_extents.x() : -half_extents.x();
      depth = dx + sphere_radius;
    } else if (dy <= dz) {
      normal_local = math::vector3{0.0f, (local.y() >= 0.0f) ? 1.0f : -1.0f, 0.0f};
      point_local.y() = (local.y() >= 0.0f) ? half_extents.y() : -half_extents.y();
      depth = dy + sphere_radius;
    } else {
      normal_local = math::vector3{0.0f, 0.0f, (local.z() >= 0.0f) ? 1.0f : -1.0f};
      point_local.z() = (local.z() >= 0.0f) ? half_extents.z() : -half_extents.z();
      depth = dz + sphere_radius;
    }
  } else {
    const auto clamped = math::vector3::max(-half_extents, math::vector3::min(local, half_extents));
    const auto diff = local - clamped;
    const auto distance = diff.length();

    if (distance >= sphere_radius) {
      return std::nullopt;
    }

    depth = sphere_radius - distance;
    normal_local = (distance > math::epsilonf) ? math::vector3{diff * (1.0f / distance)} : math::vector3::up;
    point_local = clamped;
  }

  const auto normal_world = box_rotation * normal_local;
  const auto point_world = box_center + box_rotation * point_local;

  return single_point(normal_world, point_world, depth);
}

// Returns a normal pointing from the capsule toward the sphere (capsule acts as the reference shape).
[[nodiscard]] auto sphere_capsule(const math::vector3& sphere_center, std::float_t sphere_radius, const math::vector3& capsule_center, const math::quaternion& capsule_rotation, std::float_t capsule_radius, std::float_t capsule_half_height) -> std::optional<narrow_result> {
  const auto axis = capsule_rotation * math::vector3{0.0f, 1.0f, 0.0f};
  const auto p0 = capsule_center - axis * capsule_half_height;
  const auto p1 = capsule_center + axis * capsule_half_height;

  const auto closest = closest_point_on_segment(sphere_center, p0, p1);

  // sphere_sphere(a, b) points a -> b, so (capsule-skeleton -> sphere) is exactly the convention
  // this function promises its caller.
  return sphere_sphere(closest, capsule_radius, sphere_center, sphere_radius);
}

// Returns a normal pointing from the box toward the capsule (same convention as sphere_box).
[[nodiscard]] auto capsule_box(const math::vector3& capsule_center, const math::quaternion& capsule_rotation, std::float_t capsule_radius, std::float_t capsule_half_height, const math::vector3& box_center, const math::quaternion& box_rotation, const math::vector3& half_extents) -> std::optional<narrow_result> {
  const auto axis = capsule_rotation * math::vector3{0.0f, 1.0f, 0.0f};
  const auto p0 = capsule_center - axis * capsule_half_height;
  const auto p1 = capsule_center + axis * capsule_half_height;

  // Closest point on the capsule's core segment to the box: alternate "clamp into the box" and
  // "closest point on the segment to that clamped point" -- both steps are projections onto a
  // convex set, so this converges to the true closest pair after a handful of iterations.
  auto closest = capsule_center;

  for (auto i = 0; i < 4; ++i) {
    const auto local = math::quaternion::conjugate(box_rotation) * (closest - box_center);
    const auto clamped_local = math::vector3::max(-half_extents, math::vector3::min(local, half_extents));
    const auto clamped_world = box_center + box_rotation * clamped_local;
    closest = closest_point_on_segment(clamped_world, p0, p1);
  }

  return sphere_box(closest, capsule_radius, box_center, box_rotation, half_extents);
}

[[nodiscard]] auto capsule_capsule(const math::vector3& center_a, const math::quaternion& rotation_a, std::float_t radius_a, std::float_t half_height_a, const math::vector3& center_b, const math::quaternion& rotation_b, std::float_t radius_b, std::float_t half_height_b) -> std::optional<narrow_result> {
  const auto axis_a = rotation_a * math::vector3{0.0f, 1.0f, 0.0f};
  const auto axis_b = rotation_b * math::vector3{0.0f, 1.0f, 0.0f};

  const auto a0 = center_a - axis_a * half_height_a;
  const auto a1 = center_a + axis_a * half_height_a;
  const auto b0 = center_b - axis_b * half_height_b;
  const auto b1 = center_b + axis_b * half_height_b;

  auto closest_a = math::vector3::zero;
  auto closest_b = math::vector3::zero;

  closest_points_segment_segment(a0, a1, b0, b1, closest_a, closest_b);

  return sphere_sphere(closest_a, radius_a, closest_b, radius_b);
}

// -- Box-box: SAT + face clipping --------------------------------------------------------------

struct clip_plane {
  math::vector3 normal;
  std::float_t offset; // keep points with dot(normal, point) <= offset
}; // struct clip_plane

[[nodiscard]] auto clip_against_plane(const std::vector<math::vector3>& input, const clip_plane& plane) -> std::vector<math::vector3> {
  if (input.empty()) {
    return {};
  }

  auto output = std::vector<math::vector3>{};
  output.reserve(input.size() + 1u);

  for (auto index = std::size_t{0}; index < input.size(); ++index) {
    const auto& current = input[index];
    const auto& previous = input[(index + input.size() - 1u) % input.size()];

    const auto current_side = math::vector3::dot(plane.normal, current) - plane.offset;
    const auto previous_side = math::vector3::dot(plane.normal, previous) - plane.offset;

    if (current_side <= 0.0f) {
      if (previous_side > 0.0f) {
        const auto t = previous_side / (previous_side - current_side);
        output.push_back(previous + (current - previous) * t);
      }

      output.push_back(current);
    } else if (previous_side <= 0.0f) {
      const auto t = previous_side / (previous_side - current_side);
      output.push_back(previous + (current - previous) * t);
    }
  }

  return output;
}

[[nodiscard]] auto face_corners(const math::vector3& center, const std::array<math::vector3, 3>& axes, const math::vector3& half_extents, std::uint32_t axis_index, std::float_t sign) -> std::array<math::vector3, 4> {
  const auto u_index = (axis_index + 1u) % 3u;
  const auto v_index = (axis_index + 2u) % 3u;

  const auto face_center = center + axes[axis_index] * (sign * half_extents[axis_index]);
  const auto u = axes[u_index] * half_extents[u_index];
  const auto v = axes[v_index] * half_extents[v_index];

  return {
    face_center + u + v,
    face_center + u - v,
    face_center - u - v,
    face_center - u + v
  };
}

[[nodiscard]] auto box_box(const math::vector3& center_a, const math::quaternion& rotation_a, const math::vector3& half_extents_a, const math::vector3& center_b, const math::quaternion& rotation_b, const math::vector3& half_extents_b) -> std::optional<narrow_result> {
  const auto axes_a = std::array<math::vector3, 3>{rotation_a * math::vector3{1.0f, 0.0f, 0.0f}, rotation_a * math::vector3{0.0f, 1.0f, 0.0f}, rotation_a * math::vector3{0.0f, 0.0f, 1.0f}};
  const auto axes_b = std::array<math::vector3, 3>{rotation_b * math::vector3{1.0f, 0.0f, 0.0f}, rotation_b * math::vector3{0.0f, 1.0f, 0.0f}, rotation_b * math::vector3{0.0f, 0.0f, 1.0f}};

  const auto center_delta = center_b - center_a;

  enum class axis_kind : std::uint8_t { face_a, face_b, edge };

  auto best_overlap = std::numeric_limits<std::float_t>::max();
  auto best_normal = math::vector3::up;
  auto best_kind = axis_kind::face_a;
  auto best_i = std::uint32_t{0};
  auto best_j = std::uint32_t{0};

  const auto test_axis = [&](math::vector3 axis, axis_kind kind, std::uint32_t i, std::uint32_t j) -> bool {
    const auto length_squared = axis.length_squared();

    if (length_squared <= 1e-8f) {
      return true; // near-parallel edges -- not a useful separating axis, skip without rejecting
    }

    axis = axis * (1.0f / std::sqrt(length_squared));

    auto radius_a = 0.0f;
    auto radius_b = 0.0f;

    for (auto k = std::uint32_t{0}; k < 3u; ++k) {
      radius_a += half_extents_a[k] * std::abs(math::vector3::dot(axes_a[k], axis));
      radius_b += half_extents_b[k] * std::abs(math::vector3::dot(axes_b[k], axis));
    }

    const auto distance = math::vector3::dot(center_delta, axis);
    const auto overlap = radius_a + radius_b - std::abs(distance);

    if (overlap < 0.0f) {
      return false; // separating axis found -- boxes don't overlap
    }

    if (overlap < best_overlap) {
      best_overlap = overlap;
      best_normal = (distance < 0.0f) ? -axis : axis; // oriented A -> B
      best_kind = kind;
      best_i = i;
      best_j = j;
    }

    return true;
  };

  for (auto i = std::uint32_t{0}; i < 3u; ++i) {
    if (!test_axis(axes_a[i], axis_kind::face_a, i, 0u)) {
      return std::nullopt;
    }
  }

  for (auto j = std::uint32_t{0}; j < 3u; ++j) {
    if (!test_axis(axes_b[j], axis_kind::face_b, 0u, j)) {
      return std::nullopt;
    }
  }

  for (auto i = std::uint32_t{0}; i < 3u; ++i) {
    for (auto j = std::uint32_t{0}; j < 3u; ++j) {
      if (!test_axis(math::vector3::cross(axes_a[i], axes_b[j]), axis_kind::edge, i, j)) {
        return std::nullopt;
      }
    }
  }

  if (best_kind == axis_kind::edge) {
    const auto pick_edge_center = [](const math::vector3& center, const std::array<math::vector3, 3>& axes, const math::vector3& half_extents, std::uint32_t axis_index, const math::vector3& towards) -> math::vector3 {
      auto result = center;

      for (auto k = std::uint32_t{0}; k < 3u; ++k) {
        if (k == axis_index) {
          continue;
        }

        const auto sign = (math::vector3::dot(towards, axes[k]) >= 0.0f) ? 1.0f : -1.0f;
        result = result + axes[k] * (half_extents[k] * sign);
      }

      return result;
    };

    const auto edge_center_a = pick_edge_center(center_a, axes_a, half_extents_a, best_i, center_delta);
    const auto edge_center_b = pick_edge_center(center_b, axes_b, half_extents_b, best_j, -center_delta);

    const auto a0 = edge_center_a - axes_a[best_i] * half_extents_a[best_i];
    const auto a1 = edge_center_a + axes_a[best_i] * half_extents_a[best_i];
    const auto b0 = edge_center_b - axes_b[best_j] * half_extents_b[best_j];
    const auto b1 = edge_center_b + axes_b[best_j] * half_extents_b[best_j];

    auto closest_a = math::vector3::zero;
    auto closest_b = math::vector3::zero;

    closest_points_segment_segment(a0, a1, b0, b1, closest_a, closest_b);

    return single_point(best_normal, (closest_a + closest_b) * 0.5f, best_overlap);
  }

  const auto reference_is_a = (best_kind == axis_kind::face_a);

  const auto& ref_center = reference_is_a ? center_a : center_b;
  const auto& ref_axes = reference_is_a ? axes_a : axes_b;
  const auto& ref_half_extents = reference_is_a ? half_extents_a : half_extents_b;
  const auto ref_axis_index = reference_is_a ? best_i : best_j;

  const auto& incident_center = reference_is_a ? center_b : center_a;
  const auto& incident_axes = reference_is_a ? axes_b : axes_a;
  const auto& incident_half_extents = reference_is_a ? half_extents_b : half_extents_a;

  // best_normal points A -> B; the reference face's own outward normal points away from the
  // reference box, which is the same direction only when the reference box is A.
  const auto ref_outward = reference_is_a ? best_normal : -best_normal;
  const auto ref_sign = (math::vector3::dot(ref_outward, ref_axes[ref_axis_index]) >= 0.0f) ? 1.0f : -1.0f;

  auto incident_axis_index = std::uint32_t{0};
  auto incident_sign = 1.0f;
  auto best_incident_dot = std::numeric_limits<std::float_t>::max();

  for (auto k = std::uint32_t{0}; k < 3u; ++k) {
    for (const auto sign : {1.0f, -1.0f}) {
      const auto d = math::vector3::dot(incident_axes[k] * sign, ref_outward);

      if (d < best_incident_dot) {
        best_incident_dot = d;
        incident_axis_index = k;
        incident_sign = sign;
      }
    }
  }

  const auto incident_corners = face_corners(incident_center, incident_axes, incident_half_extents, incident_axis_index, incident_sign);

  const auto u_index = (ref_axis_index + 1u) % 3u;
  const auto v_index = (ref_axis_index + 2u) % 3u;

  const auto planes = std::array<clip_plane, 4>{
    clip_plane{ref_axes[u_index], math::vector3::dot(ref_axes[u_index], ref_center) + ref_half_extents[u_index]},
    clip_plane{-ref_axes[u_index], -math::vector3::dot(ref_axes[u_index], ref_center) + ref_half_extents[u_index]},
    clip_plane{ref_axes[v_index], math::vector3::dot(ref_axes[v_index], ref_center) + ref_half_extents[v_index]},
    clip_plane{-ref_axes[v_index], -math::vector3::dot(ref_axes[v_index], ref_center) + ref_half_extents[v_index]}
  };

  auto polygon = std::vector<math::vector3>{incident_corners.begin(), incident_corners.end()};

  for (const auto& plane : planes) {
    polygon = clip_against_plane(polygon, plane);

    if (polygon.empty()) {
      break;
    }
  }

  const auto ref_face_center = ref_center + ref_axes[ref_axis_index] * (ref_sign * ref_half_extents[ref_axis_index]);

  auto candidates = std::vector<narrow_point>{};
  candidates.reserve(polygon.size());

  for (const auto& vertex : polygon) {
    const auto depth = math::vector3::dot(ref_outward, ref_face_center - vertex);

    if (depth >= 0.0f) {
      candidates.push_back(narrow_point{vertex, depth});
    }
  }

  if (candidates.empty()) {
    // Numerical edge case: clipping (or the depth filter) discarded everything. Fall back to the
    // single deepest pre-clip incident vertex so a confirmed SAT overlap never yields zero points.
    auto deepest = incident_corners[0];
    auto deepest_depth = math::vector3::dot(ref_outward, ref_face_center - deepest);

    for (const auto& vertex : incident_corners) {
      const auto depth = math::vector3::dot(ref_outward, ref_face_center - vertex);

      if (depth > deepest_depth) {
        deepest_depth = depth;
        deepest = vertex;
      }
    }

    candidates.push_back(narrow_point{deepest, std::max(deepest_depth, best_overlap)});
  }

  if (candidates.size() > max_manifold_points) {
    std::ranges::partial_sort(candidates, candidates.begin() + max_manifold_points, [](const narrow_point& lhs, const narrow_point& rhs) {
      return lhs.depth > rhs.depth;
    });

    candidates.resize(max_manifold_points);
  }

  auto result = narrow_result{best_normal, {}};

  for (const auto& candidate : candidates) {
    result.points.push_back(candidate);
  }

  return result;
}

// -- Generic fallback: GJK + EPA -----------------------------------------------------------------

[[nodiscard]] auto generic_gjk_epa(const convex_shape& shape_a, const transform& pose_a, const convex_shape& shape_b, const transform& pose_b) -> std::optional<narrow_result> {
  const auto gjk = gjk_intersect(shape_a, pose_a, shape_b, pose_b);

  if (!gjk.intersecting) {
    return std::nullopt;
  }

  const auto epa = epa_penetration(shape_a, pose_a, shape_b, pose_b, gjk.simplex);

  if (!epa.valid) {
    return std::nullopt;
  }

  const auto point = (epa.point_on_a + epa.point_on_b) * 0.5f;

  return single_point(epa.normal, point, epa.penetration_depth);
}

// -- Dispatch -------------------------------------------------------------------------------------

[[nodiscard]] auto dispatch(const convex_shape& shape_a, const transform& pose_a, const convex_shape& shape_b, const transform& pose_b) -> std::optional<narrow_result> {
  const auto index_a = shape_a.index();
  const auto index_b = shape_b.index();

  constexpr auto sphere_index = std::size_t{0};
  constexpr auto capsule_index = std::size_t{2};
  constexpr auto box_index = std::size_t{3};

  if (index_a == sphere_index && index_b == sphere_index) {
    return sphere_sphere(pose_a.position, std::get<sphere>(shape_a).radius, pose_b.position, std::get<sphere>(shape_b).radius);
  }

  if (index_a == sphere_index && index_b == box_index) {
    auto result = sphere_box(pose_a.position, std::get<sphere>(shape_a).radius, pose_b.position, pose_b.rotation, std::get<box>(shape_b).half_extents);
    if (result) { result->normal = -result->normal; }
    return result;
  }

  if (index_a == box_index && index_b == sphere_index) {
    return sphere_box(pose_b.position, std::get<sphere>(shape_b).radius, pose_a.position, pose_a.rotation, std::get<box>(shape_a).half_extents);
  }

  if (index_a == sphere_index && index_b == capsule_index) {
    const auto& capsule_shape = std::get<capsule>(shape_b);
    auto result = sphere_capsule(pose_a.position, std::get<sphere>(shape_a).radius, pose_b.position, pose_b.rotation, capsule_shape.radius, capsule_shape.half_height);
    if (result) { result->normal = -result->normal; }
    return result;
  }

  if (index_a == capsule_index && index_b == sphere_index) {
    const auto& capsule_shape = std::get<capsule>(shape_a);
    return sphere_capsule(pose_b.position, std::get<sphere>(shape_b).radius, pose_a.position, pose_a.rotation, capsule_shape.radius, capsule_shape.half_height);
  }

  if (index_a == capsule_index && index_b == capsule_index) {
    const auto& capsule_a = std::get<capsule>(shape_a);
    const auto& capsule_b = std::get<capsule>(shape_b);
    return capsule_capsule(pose_a.position, pose_a.rotation, capsule_a.radius, capsule_a.half_height, pose_b.position, pose_b.rotation, capsule_b.radius, capsule_b.half_height);
  }

  if (index_a == capsule_index && index_b == box_index) {
    const auto& capsule_shape = std::get<capsule>(shape_a);
    auto result = capsule_box(pose_a.position, pose_a.rotation, capsule_shape.radius, capsule_shape.half_height, pose_b.position, pose_b.rotation, std::get<box>(shape_b).half_extents);
    if (result) { result->normal = -result->normal; }
    return result;
  }

  if (index_a == box_index && index_b == capsule_index) {
    const auto& capsule_shape = std::get<capsule>(shape_b);
    return capsule_box(pose_b.position, pose_b.rotation, capsule_shape.radius, capsule_shape.half_height, pose_a.position, pose_a.rotation, std::get<box>(shape_a).half_extents);
  }

  if (index_a == box_index && index_b == box_index) {
    const auto& box_a = std::get<box>(shape_a);
    const auto& box_b = std::get<box>(shape_b);
    return box_box(pose_a.position, pose_a.rotation, box_a.half_extents, pose_b.position, pose_b.rotation, box_b.half_extents);
  }

  return generic_gjk_epa(shape_a, pose_a, shape_b, pose_b);
}

// Shared by every generate_*_contact below: turns a raw dispatch()/combined mesh result into the
// contact_manifold the solver expects, deriving each point's torque anchors from node_a/node_b's
// own local_transform::position.
[[nodiscard]] auto build_manifold(const scenes::node& node_a, const scenes::node& node_b, const scenes::local_transform& local_a, const scenes::local_transform& local_b, std::float_t friction_a, std::float_t friction_b, std::float_t restitution_a, std::float_t restitution_b, const narrow_result& raw) -> std::optional<contact_manifold> {
  auto manifold = contact_manifold{};
  manifold.node_a = node_a;
  manifold.node_b = node_b;
  manifold.normal = raw.normal;
  manifold.combined_friction = std::sqrt(std::max(friction_a, 0.0f) * std::max(friction_b, 0.0f));
  manifold.combined_restitution = std::max(restitution_a, restitution_b);

  for (const auto& point : raw.points) {
    if (manifold.points.is_full()) {
      break;
    }

    manifold.points.push_back(contact_point{
      point.point,
      point.depth,
      point.point - local_a.position,
      point.point - local_b.position,
      0.0f, 0.0f, 0.0f,
      static_cast<std::uint32_t>(manifold.points.size())
    });
  }

  if (manifold.points.is_empty()) {
    return std::nullopt;
  }

  return manifold;
}

auto generate_contact(const sbx::scenes::node& node_a, const sbx::scenes::node& node_b) -> std::optional<contact_manifold> {
  const auto& collider_a = node_a.get_component<shape_collider>();
  const auto& collider_b = node_b.get_component<shape_collider>();
  const auto& local_a = node_a.get_component<scenes::local_transform>();
  const auto& local_b = node_b.get_component<scenes::local_transform>();

  const auto pose_a = compose_pose(local_a, collider_a.offset, collider_a.rotation);
  const auto pose_b = compose_pose(local_b, collider_b.offset, collider_b.rotation);

  const auto raw = dispatch(collider_a.shape, pose_a, collider_b.shape, pose_b);

  if (!raw) {
    return std::nullopt;
  }

  return build_manifold(node_a, node_b, local_a, local_b, collider_a.friction, collider_b.friction, collider_a.restitution, collider_b.restitution, *raw);
}

// -- Mesh colliders ------------------------------------------------------------------------------

struct resolved_convex {
  convex_shape shape;
  transform pose;
  std::float_t friction;
  std::float_t restitution;
}; // struct resolved_convex

// A node participates in narrowphase as an ordinary convex_shape either by authoring one directly
// (shape_collider) or, for a mesh_collider with convex == true, via its cached hull point set --
// from here on the two are indistinguishable to dispatch()/GJK/EPA. Returns nullopt for a
// non-convex mesh_collider (it can never stand in as a single convex_shape -- see
// generate_mesh_contact) or an unresolvable one (no mesh assigned, or an empty cached hull).
[[nodiscard]] auto resolve_convex(const scenes::node& node, convex_hull_cache& hull_cache, assets::assets_module& assets_module) -> std::optional<resolved_convex> {
  const auto& local = node.get_component<scenes::local_transform>();

  if (node.has_component<shape_collider>()) {
    const auto& collider = node.get_component<shape_collider>();
    return resolved_convex{collider.shape, compose_pose(local, collider.offset, collider.rotation), collider.friction, collider.restitution};
  }

  if (node.has_component<mesh_collider>()) {
    const auto& collider = node.get_component<mesh_collider>();

    if (!collider.is_convex || !collider.mesh.is_valid()) {
      return std::nullopt;
    }

    const auto& hull_data = hull_cache.get_or_build(assets_module, collider.mesh->id());

    if (hull_data.points.is_empty()) {
      return std::nullopt;
    }

    return resolved_convex{convex_shape{convex_hull{hull_data.points}}, compose_pose(local, collider.offset, collider.rotation), collider.friction, collider.restitution};
  }

  return std::nullopt;
}

// Both sides resolve to an ordinary convex_shape (shape_collider or a convex mesh_collider, in any
// combination) -- a single ordinary GJK/EPA-or-closed-form call via dispatch(), same as
// generate_contact but for whichever of the two collider kinds each side turns out to be.
[[nodiscard]] auto generate_convex_pair_contact(const scenes::node& node_a, const scenes::node& node_b, convex_hull_cache& hull_cache, assets::assets_module& assets_module) -> std::optional<contact_manifold> {
  const auto resolved_a = resolve_convex(node_a, hull_cache, assets_module);
  const auto resolved_b = resolve_convex(node_b, hull_cache, assets_module);

  if (!resolved_a || !resolved_b) {
    return std::nullopt;
  }

  const auto raw = dispatch(resolved_a->shape, resolved_a->pose, resolved_b->shape, resolved_b->pose);

  if (!raw) {
    return std::nullopt;
  }

  const auto& local_a = node_a.get_component<scenes::local_transform>();
  const auto& local_b = node_b.get_component<scenes::local_transform>();

  return build_manifold(node_a, node_b, local_a, local_b, resolved_a->friction, resolved_b->friction, resolved_a->restitution, resolved_b->restitution, *raw);
}

// shape_node resolves as an ordinary convex_shape (via resolve_convex -- so this also covers a
// convex mesh_collider landing on a non-convex one); mesh_node is a non-convex mesh_collider, tested
// per-candidate-triangle against its mesh_collision_cache BVH. Builds one combined manifold for the
// pair: the deepest touching triangle's normal becomes the manifold's normal (exact when the
// touched triangles are coplanar, e.g. a flat floor region -- the common case; an approximation
// otherwise, the same "internal edge" v1 limitation noted when mesh narrowphase was first added),
// and up to max_manifold_points deepest points across every touching triangle become its points.
[[nodiscard]] auto generate_mesh_contact(const scenes::node& shape_node, const scenes::node& mesh_node, mesh_collision_cache& mesh_cache, convex_hull_cache& hull_cache, assets::assets_module& assets_module) -> std::optional<contact_manifold> {
  const auto resolved_shape = resolve_convex(shape_node, hull_cache, assets_module);

  if (!resolved_shape) {
    return std::nullopt;
  }

  const auto& mesh_collider_component = mesh_node.get_component<mesh_collider>();

  if (!mesh_collider_component.mesh.is_valid()) {
    return std::nullopt;
  }

  const auto& local_mesh = mesh_node.get_component<scenes::local_transform>();
  const auto pose_mesh = compose_pose(local_mesh, mesh_collider_component.offset, mesh_collider_component.rotation);

  const auto& mesh_data = mesh_cache.get_or_build(assets_module, mesh_collider_component.mesh->id());

  if (mesh_data.indices.empty()) {
    return std::nullopt;
  }

  // The shape's world AABB, transformed into the mesh's local space (rotation+translation only, so
  // the inverse is just conjugate-rotate-then-translate) to query the triangle BVH.
  auto shape_world_aabb = math::volume{};

  for (const auto& corner : local_aabb(resolved_shape->shape).corners()) {
    shape_world_aabb.include(resolved_shape->pose.position + resolved_shape->pose.rotation * corner);
  }

  auto local_query_aabb = math::volume{};

  for (const auto& corner : shape_world_aabb.corners()) {
    local_query_aabb.include(math::quaternion::conjugate(pose_mesh.rotation) * (corner - pose_mesh.position));
  }

  auto triangle_results = std::vector<narrow_result>{};

  mesh_data.triangle_bvh.query(local_query_aabb, [&](std::uint32_t triangle_index) {
    const auto i0 = mesh_data.indices[triangle_index * 3u + 0u];
    const auto i1 = mesh_data.indices[triangle_index * 3u + 1u];
    const auto i2 = mesh_data.indices[triangle_index * 3u + 2u];

    const auto candidate_triangle = convex_shape{triangle{mesh_data.vertices[i0], mesh_data.vertices[i1], mesh_data.vertices[i2]}};

    if (const auto result = dispatch(resolved_shape->shape, resolved_shape->pose, candidate_triangle, pose_mesh)) {
      triangle_results.push_back(*result);
    }
  });

  if (triangle_results.empty()) {
    return std::nullopt;
  }

  struct mesh_candidate {
    math::vector3 normal;
    narrow_point point;
  }; // struct mesh_candidate

  auto candidates = std::vector<mesh_candidate>{};

  for (const auto& result : triangle_results) {
    for (const auto& point : result.points) {
      candidates.push_back(mesh_candidate{result.normal, point});
    }
  }

  std::ranges::sort(candidates, [](const mesh_candidate& lhs, const mesh_candidate& rhs) {
    return lhs.point.depth > rhs.point.depth;
  });

  if (candidates.size() > max_manifold_points) {
    candidates.resize(max_manifold_points);
  }

  auto combined = narrow_result{candidates.front().normal, {}};

  for (const auto& candidate : candidates) {
    combined.points.push_back(candidate.point);
  }

  const auto& local_shape = shape_node.get_component<scenes::local_transform>();

  return build_manifold(shape_node, mesh_node, local_shape, local_mesh, resolved_shape->friction, mesh_collider_component.friction, resolved_shape->restitution, mesh_collider_component.restitution, combined);
}

[[nodiscard]] auto flip_manifold(contact_manifold manifold, const scenes::node& node_a, const scenes::node& node_b) -> contact_manifold {
  manifold.node_a = node_a;
  manifold.node_b = node_b;
  manifold.normal = -manifold.normal;

  for (auto& point : manifold.points) {
    std::swap(point.anchor_a, point.anchor_b);
  }

  return manifold;
}

auto generate_pair_contact(const sbx::scenes::node& node_a, const sbx::scenes::node& node_b, mesh_collision_cache& mesh_cache, convex_hull_cache& hull_cache, assets::assets_module& assets_module) -> std::optional<contact_manifold> {
  const auto a_is_raw_mesh = node_a.has_component<mesh_collider>() && !node_a.get_component<mesh_collider>().is_convex;
  const auto b_is_raw_mesh = node_b.has_component<mesh_collider>() && !node_b.get_component<mesh_collider>().is_convex;

  if (a_is_raw_mesh && b_is_raw_mesh) {
    return std::nullopt; // two non-convex mesh colliders never collide, matching Unity
  }

  if (a_is_raw_mesh) {
    const auto result = generate_mesh_contact(node_b, node_a, mesh_cache, hull_cache, assets_module);
    return result ? std::optional{flip_manifold(*result, node_a, node_b)} : std::nullopt;
  }

  if (b_is_raw_mesh) {
    return generate_mesh_contact(node_a, node_b, mesh_cache, hull_cache, assets_module);
  }

  if (!node_a.has_component<mesh_collider>() && !node_b.has_component<mesh_collider>()) {
    return generate_contact(node_a, node_b); // common hot path: neither side is a mesh at all
  }

  return generate_convex_pair_contact(node_a, node_b, hull_cache, assets_module);
}

} // namespace sbx::physics
