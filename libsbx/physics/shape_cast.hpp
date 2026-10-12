// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_SHAPE_CAST_HPP_
#define LIBSBX_PHYSICS_SHAPE_CAST_HPP_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>

#include <libsbx/math/constants.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/terrain/heightmap.hpp>

#include <libsbx/physics/gjk.hpp>
#include <libsbx/physics/mesh_collision_cache.hpp>
#include <libsbx/physics/shapes.hpp>

namespace sbx::physics {

struct shape_cast_hit {
  std::float_t distance{0.0f}; // along the cast direction; 0 when initially overlapping
  math::vector3 point{math::vector3::zero};  // world space, on the target
  math::vector3 normal{math::vector3::up};   // world space, unit, the target's surface normal (toward the cast shape)
  bool initially_overlapping{false};
}; // struct shape_cast_hit

/** @brief The world AABB of a posed convex shape. */
[[nodiscard]] inline auto world_aabb(const convex_shape& shape, const transform& pose) -> math::volume {
  auto result = math::volume{};

  for (const auto& corner : local_aabb(shape).corners()) {
    result.include(pose.position + pose.rotation * (corner * pose.scale));
  }

  return result;
}

/** @brief The AABB covering a shape over a linear sweep. */
[[nodiscard]] inline auto swept_aabb(const convex_shape& shape, const transform& pose, const math::vector3& displacement) -> math::volume {
  auto end_pose = pose;
  end_pose.position = pose.position + displacement;

  auto result = world_aabb(shape, pose);
  result.include(world_aabb(shape, end_pose));

  return result;
}

/**
 * @brief Calls visit(const triangle&) with every terrain triangle, in world space, whose cell overlaps @p world_bounds.
 *
 * The heightmap is centered on the world origin, the same assumption raycast_heightfield makes.
 */
template<typename Visit>
auto for_each_heightfield_triangle(const terrain::heightmap& map, const math::volume& world_bounds, Visit&& visit) -> void {
  if (map.width() < 2u || map.depth() < 2u || map.cell_size() <= math::epsilonf) {
    return;
  }

  const auto extent = map.half_extent();
  const auto cell = map.cell_size();
  const auto last_x = static_cast<std::float_t>(map.width() - 2u);
  const auto last_z = static_cast<std::float_t>(map.depth() - 2u);

  const auto min_x = std::floor((world_bounds.min().x() + extent.x()) / cell);
  const auto max_x = std::floor((world_bounds.max().x() + extent.x()) / cell);
  const auto min_z = std::floor((world_bounds.min().z() + extent.y()) / cell);
  const auto max_z = std::floor((world_bounds.max().z() + extent.y()) / cell);

  if (max_x < 0.0f || max_z < 0.0f || min_x > last_x || min_z > last_z) {
    return;
  }

  const auto x0 = static_cast<std::uint32_t>(std::max(min_x, 0.0f));
  const auto x1 = static_cast<std::uint32_t>(std::min(max_x, last_x));
  const auto z0 = static_cast<std::uint32_t>(std::max(min_z, 0.0f));
  const auto z1 = static_cast<std::uint32_t>(std::min(max_z, last_z));

  const auto vertex = [&](std::uint32_t x, std::uint32_t z) {
    return math::vector3{static_cast<std::float_t>(x) * cell - extent.x(), map.height_at(x, z), static_cast<std::float_t>(z) * cell - extent.y()};
  };

  for (auto z = z0; z <= z1; ++z) {
    for (auto x = x0; x <= x1; ++x) {
      const auto p00 = vertex(x, z);
      const auto p10 = vertex(x + 1u, z);
      const auto p01 = vertex(x, z + 1u);
      const auto p11 = vertex(x + 1u, z + 1u);

      const auto lowest = std::min({p00.y(), p10.y(), p01.y(), p11.y()});
      const auto highest = std::max({p00.y(), p10.y(), p01.y(), p11.y()});

      if (highest < world_bounds.min().y() || lowest > world_bounds.max().y()) {
        continue;
      }

      std::invoke(visit, triangle{p00, p01, p11});
      std::invoke(visit, triangle{p00, p11, p10});
    }
  }
}

/**
 * @brief Sweeps @p shape from @p pose along @p direction (unit) up to @p max_distance against one target, by conservative advancement. Rotation is held fixed.
 *
 * @return The first contact, or nullopt if the sweep misses.
 */
[[nodiscard]] auto cast_convex(const convex_shape& shape, const transform& pose, const math::vector3& direction, std::float_t max_distance, const convex_shape& target, const transform& target_pose) -> std::optional<shape_cast_hit>;

/** @brief Same as cast_convex, against every triangle of a non-convex mesh the sweep's bounds reach. */
[[nodiscard]] auto cast_against_triangle_mesh(const convex_shape& shape, const transform& pose, const math::vector3& direction, std::float_t max_distance, const mesh_collision_data& mesh, const transform& mesh_pose) -> std::optional<shape_cast_hit>;

/** @brief Same as cast_convex, against the terrain triangles under the sweep. */
[[nodiscard]] auto cast_against_heightfield(const convex_shape& shape, const transform& pose, const math::vector3& direction, std::float_t max_distance, const terrain::heightmap& map) -> std::optional<shape_cast_hit>;

/** @brief Calls visit(const triangle&, const transform& mesh_pose) with every mesh triangle whose BVH leaf overlaps @p world_bounds. Triangles are in the mesh's local space. */
template<typename Visit>
auto for_each_mesh_triangle(const mesh_collision_data& mesh, const transform& mesh_pose, const math::volume& world_bounds, Visit&& visit) -> void {
  if (mesh.indices.empty()) {
    return;
  }

  const auto inverse_scale = math::vector3{
    (mesh_pose.scale.x() > math::epsilonf) ? (1.0f / mesh_pose.scale.x()) : 0.0f,
    (mesh_pose.scale.y() > math::epsilonf) ? (1.0f / mesh_pose.scale.y()) : 0.0f,
    (mesh_pose.scale.z() > math::epsilonf) ? (1.0f / mesh_pose.scale.z()) : 0.0f
  };

  auto local_bounds = math::volume{};

  for (const auto& corner : world_bounds.corners()) {
    local_bounds.include((math::quaternion::conjugate(mesh_pose.rotation) * (corner - mesh_pose.position)) * inverse_scale);
  }

  mesh.triangle_bvh.query(local_bounds, [&](std::uint32_t triangle_index) {
    const auto i0 = mesh.indices[triangle_index * 3u + 0u];
    const auto i1 = mesh.indices[triangle_index * 3u + 1u];
    const auto i2 = mesh.indices[triangle_index * 3u + 2u];

    std::invoke(visit, triangle{mesh.vertices[i0], mesh.vertices[i1], mesh.vertices[i2]}, mesh_pose);
  });
}

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_SHAPE_CAST_HPP_
