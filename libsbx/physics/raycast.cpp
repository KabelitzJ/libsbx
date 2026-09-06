// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/raycast.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/constants.hpp>
#include <libsbx/math/vector2.hpp>

#include <libsbx/utility/overload.hpp>

namespace sbx::physics {

[[nodiscard]] auto divide_componentwise(const math::vector3& lhs, const math::vector3& rhs) -> math::vector3 {
  return math::vector3{lhs.x() / rhs.x(), lhs.y() / rhs.y(), lhs.z() / rhs.z()};
}

/**
 * @brief One candidate ray hit in local space: parameter @p t (same units as the world ray's,
 * since local_ray below preserves the world parametrization -- see world_ray_to_local's doc
 * comment) and the local-space outward normal at that point.
 */
struct local_hit {
  std::float_t t;
  math::vector3 normal;
}; // struct local_hit

/**
 * @brief Ray-vs-sphere, centered at the local origin. Handles the ray starting inside the sphere
 * (returns the exit point, t2) as well as outside (t1).
 */
[[nodiscard]] auto raycast_sphere_local(const math::vector3& origin, const math::vector3& dir, std::float_t radius) -> std::optional<local_hit> {
  const auto a = math::vector3::dot(dir, dir);
  const auto b = 2.0f * math::vector3::dot(origin, dir);
  const auto c = math::vector3::dot(origin, origin) - radius * radius;

  const auto discriminant = b * b - 4.0f * a * c;

  if (discriminant < 0.0f || a <= math::epsilonf) {
    return std::nullopt;
  }

  const auto sqrt_discriminant = std::sqrt(discriminant);
  const auto t0 = (-b - sqrt_discriminant) / (2.0f * a);
  const auto t1 = (-b + sqrt_discriminant) / (2.0f * a);

  const auto t = (t0 >= 0.0f) ? t0 : t1;

  if (t < 0.0f) {
    return std::nullopt;
  }

  const auto point = origin + dir * t;

  return local_hit{t, math::vector3::normalized(point)};
}

/**
 * @brief Ray-vs-axis-aligned-box, centered at the local origin -- the standard slab method. If the
 * ray starts inside the box, returns a hit at t == 0 with an arbitrary (+Y) normal -- an accepted
 * simplification, since a placement/picking ray starting inside solid geometry isn't a case this
 * system needs to resolve precisely.
 */
[[nodiscard]] auto raycast_box_local(const math::vector3& origin, const math::vector3& dir, const math::vector3& half_extents) -> std::optional<local_hit> {
  auto t_min = -std::numeric_limits<std::float_t>::max();
  auto t_max = std::numeric_limits<std::float_t>::max();
  auto hit_normal = math::vector3{0.0f, 1.0f, 0.0f};

  for (auto axis = std::size_t{0}; axis < 3u; ++axis) {
    const auto origin_axis = (axis == 0u) ? origin.x() : (axis == 1u) ? origin.y() : origin.z();
    const auto dir_axis = (axis == 0u) ? dir.x() : (axis == 1u) ? dir.y() : dir.z();
    const auto extent_axis = (axis == 0u) ? half_extents.x() : (axis == 1u) ? half_extents.y() : half_extents.z();

    if (std::abs(dir_axis) <= math::epsilonf) {
      if (origin_axis < -extent_axis || origin_axis > extent_axis) {
        return std::nullopt; // parallel to this slab and outside it -- never enters the box
      }
      continue;
    }

    const auto inverse_dir = 1.0f / dir_axis;
    const auto t_to_negative_face = (-extent_axis - origin_axis) * inverse_dir;
    const auto t_to_positive_face = (extent_axis - origin_axis) * inverse_dir;

    auto near_t = t_to_negative_face;
    auto far_t = t_to_positive_face;
    auto near_sign = -1.0f;

    if (near_t > far_t) {
      std::swap(near_t, far_t);
      near_sign = 1.0f;
    }

    if (near_t > t_min) {
      t_min = near_t;
      hit_normal = math::vector3{(axis == 0u) ? near_sign : 0.0f, (axis == 1u) ? near_sign : 0.0f, (axis == 2u) ? near_sign : 0.0f};
    }

    t_max = std::min(t_max, far_t);
  }

  if (t_min > t_max || t_max < 0.0f) {
    return std::nullopt;
  }

  return local_hit{std::max(t_min, 0.0f), hit_normal};
}

/**
 * @brief Ray-vs-infinite-cylinder (axis Y) lateral wall only, restricted to |y| <= half_height --
 * shared by both raycast_cylinder_local and the capsule's central segment (raycast_capsule_local),
 * which is exactly the same wall.
 */
[[nodiscard]] auto raycast_cylinder_wall_local(const math::vector3& origin, const math::vector3& dir, std::float_t radius, std::float_t half_height) -> std::optional<local_hit> {
  const auto a = dir.x() * dir.x() + dir.z() * dir.z();

  if (a <= math::epsilonf) {
    return std::nullopt; // parallel to the axis -- never crosses the lateral wall (see shapes.hpp's cylinder doc comment for the axis convention)
  }

  const auto b = 2.0f * (origin.x() * dir.x() + origin.z() * dir.z());
  const auto c = origin.x() * origin.x() + origin.z() * origin.z() - radius * radius;

  const auto discriminant = b * b - 4.0f * a * c;

  if (discriminant < 0.0f) {
    return std::nullopt;
  }

  const auto sqrt_discriminant = std::sqrt(discriminant);
  const auto candidates = std::array{(-b - sqrt_discriminant) / (2.0f * a), (-b + sqrt_discriminant) / (2.0f * a)};

  for (const auto t : candidates) {
    if (t < 0.0f) {
      continue;
    }

    const auto y = origin.y() + t * dir.y();

    if (y >= -half_height && y <= half_height) {
      const auto point = origin + dir * t;
      return local_hit{t, math::vector3::normalized(math::vector3{point.x(), 0.0f, point.z()})};
    }
  }

  return std::nullopt;
}

[[nodiscard]] auto raycast_cylinder_local(const math::vector3& origin, const math::vector3& dir, std::float_t radius, std::float_t half_height) -> std::optional<local_hit> {
  auto best = raycast_cylinder_wall_local(origin, dir, radius, half_height);

  const auto try_cap = [&](std::float_t cap_y, const math::vector3& normal) {
    if (std::abs(dir.y()) <= math::epsilonf) {
      return;
    }

    const auto t = (cap_y - origin.y()) / dir.y();

    if (t < 0.0f || (best && t >= best->t)) {
      return;
    }

    const auto x = origin.x() + t * dir.x();
    const auto z = origin.z() + t * dir.z();

    if (x * x + z * z <= radius * radius) {
      best = local_hit{t, normal};
    }
  };

  try_cap(half_height, math::vector3{0.0f, 1.0f, 0.0f});
  try_cap(-half_height, math::vector3{0.0f, -1.0f, 0.0f});

  return best;
}

/**
 * @brief Ray-vs-capsule (axis Y, hemispherical caps beyond +-half_height) as the union of the
 * shared cylindrical wall (raycast_cylinder_wall_local) and two full spheres centered at the cap
 * centers, each accepted only where its hit point actually lies on the hemisphere (|y| beyond
 * half_height) rather than the part of the sphere that would poke into the cylindrical section --
 * correct because the capsule's true boundary is exactly the wall for |y| <= half_height joined
 * with the hemispheres beyond it.
 */
[[nodiscard]] auto raycast_capsule_local(const math::vector3& origin, const math::vector3& dir, std::float_t radius, std::float_t half_height) -> std::optional<local_hit> {
  auto best = raycast_cylinder_wall_local(origin, dir, radius, half_height);

  const auto try_cap_sphere = [&](std::float_t center_y, bool (*beyond)(std::float_t, std::float_t)) {
    const auto center = math::vector3{0.0f, center_y, 0.0f};
    const auto sphere_hit = raycast_sphere_local(origin - center, dir, radius);

    if (!sphere_hit) {
      return;
    }

    if (best && sphere_hit->t >= best->t) {
      return;
    }

    const auto point_y = origin.y() + sphere_hit->t * dir.y();

    if (beyond(point_y, half_height)) {
      best = sphere_hit;
    }
  };

  try_cap_sphere(half_height, [](std::float_t y, std::float_t h) { return y >= h; });
  try_cap_sphere(-half_height, [](std::float_t y, std::float_t h) { return y <= -h; });

  return best;
}

auto raycast_convex_shape(const convex_shape& shape, const transform& pose, const math::ray& world_ray, std::float_t max_distance) -> std::optional<shape_raycast_hit> {
  // See raycast.hpp's doc comment: this preserves the world ray's own t-parametrization, so a hit
  // found in local space at parameter t is already the correct world-space distance -- no rescale
  // needed even under a non-uniform pose.scale.
  const auto inverse_rotation = math::quaternion::conjugate(pose.rotation);
  const auto local_origin = divide_componentwise(inverse_rotation * (world_ray.origin() - pose.position), pose.scale);
  const auto local_dir = divide_componentwise(inverse_rotation * world_ray.direction(), pose.scale);

  const auto local_result = std::visit(utility::overload(
    [&](const sphere& shape) { return raycast_sphere_local(local_origin, local_dir, shape.radius); },
    [&](const cylinder& shape) { return raycast_cylinder_local(local_origin, local_dir, shape.radius, shape.half_height); },
    [&](const capsule& shape) { return raycast_capsule_local(local_origin, local_dir, shape.radius, shape.half_height); },
    [&](const box& shape) { return raycast_box_local(local_origin, local_dir, shape.half_extents); },
    [&]([[maybe_unused]] const triangle& shape) { return std::optional<local_hit>{}; },
    [&]([[maybe_unused]] const convex_hull& shape) { return std::optional<local_hit>{}; }
  ), shape);

  if (!local_result || local_result->t > max_distance) {
    return std::nullopt;
  }

  const auto world_normal_direction = pose.rotation * divide_componentwise(local_result->normal, pose.scale);

  return shape_raycast_hit{local_result->t, world_ray.point_at(local_result->t), math::vector3::normalized(world_normal_direction)};
}

auto raycast_heightfield(const terrain::heightmap& map, const math::ray& world_ray, std::float_t max_distance) -> std::optional<shape_raycast_hit> {
  if (map.cell_size() <= math::epsilonf) {
    return std::nullopt;
  }

  const auto height_above_terrain = [&](std::float_t t) {
    const auto point = world_ray.point_at(t);
    return point.y() - map.sample_bilinear(math::vector2{point.x(), point.z()});
  };

  const auto step = map.cell_size() * 0.5f;

  auto previous_t = 0.0f;
  auto previous_diff = height_above_terrain(previous_t);

  for (auto t = step; t <= max_distance; t += step) {
    const auto diff = height_above_terrain(t);

    if ((diff <= 0.0f) != (previous_diff <= 0.0f)) {
      auto lo = previous_t;
      auto hi = t;
      auto lo_diff = previous_diff;

      // Bisection refinement -- 24 iterations comfortably exceeds float32 precision over any
      // reasonable max_distance, so this always converges well past visual/gameplay tolerance.
      for (auto iteration = 0u; iteration < 24u; ++iteration) {
        const auto mid = (lo + hi) * 0.5f;
        const auto mid_diff = height_above_terrain(mid);

        if ((mid_diff <= 0.0f) == (lo_diff <= 0.0f)) {
          lo = mid;
          lo_diff = mid_diff;
        } else {
          hi = mid;
        }
      }

      const auto hit_t = (lo + hi) * 0.5f;
      const auto hit_point = world_ray.point_at(hit_t);
      const auto hit_xz = math::vector2{hit_point.x(), hit_point.z()};

      return shape_raycast_hit{hit_t, hit_point, map.sample_normal(hit_xz)};
    }

    previous_t = t;
    previous_diff = diff;
  }

  return std::nullopt;
}

} // namespace sbx::physics
