// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/shape_cast.hpp>

#include <libsbx/physics/epa.hpp>

namespace sbx::physics {

// A sweep stops this far from the target, so a shape moved to the hit distance isn't already touching.
inline constexpr auto cast_target_separation = 1e-3f;

auto cast_convex(const convex_shape& shape, const transform& pose, const math::vector3& direction, std::float_t max_distance, const convex_shape& target, const transform& target_pose) -> std::optional<shape_cast_hit> {
  constexpr auto max_iterations = std::uint32_t{32};

  auto travelled = 0.0f;
  auto moved = pose;
  auto last_point = pose.position;
  auto last_normal = -direction;

  for (auto iteration = std::uint32_t{0}; iteration < max_iterations; ++iteration) {
    moved.position = pose.position + direction * travelled;

    const auto separation = gjk_distance(shape, moved, target, target_pose);

    if (separation.intersecting) {
      if (travelled > 0.0f) {
        return shape_cast_hit{travelled, last_point, last_normal, false}; // the last step landed within float noise of touching
      }

      // Already overlapping at the start: report the penetration normal so callers can push out.
      const auto overlap = gjk_intersect(shape, pose, target, target_pose);
      const auto penetration = overlap.intersecting ? epa_penetration(shape, pose, target, target_pose, overlap.simplex) : epa_result{};

      if (!penetration.valid) {
        return shape_cast_hit{0.0f, pose.position, -direction, true};
      }

      return shape_cast_hit{0.0f, penetration.point_on_b, -penetration.normal, true};
    }

    if (separation.distance <= cast_target_separation) {
      return shape_cast_hit{travelled, separation.point_on_b, separation.normal, false};
    }

    last_point = separation.point_on_b;
    last_normal = separation.normal;

    // separation.normal points from the target toward the shape, so approaching means moving against it.
    const auto closing_speed = -math::vector3::dot(direction, separation.normal);

    if (closing_speed <= math::epsilonf) {
      return std::nullopt;
    }

    // For pure translation, the gap can't close faster than this, so the step never overshoots.
    travelled += (separation.distance - cast_target_separation * 0.5f) / closing_speed;

    if (travelled > max_distance) {
      return std::nullopt;
    }
  }

  return std::nullopt;
}

[[nodiscard]] auto nearer_hit(const std::optional<shape_cast_hit>& current, const std::optional<shape_cast_hit>& candidate) -> std::optional<shape_cast_hit> {
  if (!candidate) {
    return current;
  }

  return (!current || candidate->distance < current->distance) ? candidate : current;
}

auto cast_against_triangle_mesh(const convex_shape& shape, const transform& pose, const math::vector3& direction, std::float_t max_distance, const mesh_collision_data& mesh, const transform& mesh_pose) -> std::optional<shape_cast_hit> {
  auto best = std::optional<shape_cast_hit>{};

  for_each_mesh_triangle(mesh, mesh_pose, swept_aabb(shape, pose, direction * max_distance), [&](const triangle& candidate, const transform& triangle_pose) {
    best = nearer_hit(best, cast_convex(shape, pose, direction, max_distance, convex_shape{candidate}, triangle_pose));
  });

  return best;
}

auto cast_against_heightfield(const convex_shape& shape, const transform& pose, const math::vector3& direction, std::float_t max_distance, const terrain::heightmap& map) -> std::optional<shape_cast_hit> {
  auto best = std::optional<shape_cast_hit>{};

  for_each_heightfield_triangle(map, swept_aabb(shape, pose, direction * max_distance), [&](const triangle& candidate) {
    best = nearer_hit(best, cast_convex(shape, pose, direction, max_distance, convex_shape{candidate}, transform{}));
  });

  return best;
}

} // namespace sbx::physics
