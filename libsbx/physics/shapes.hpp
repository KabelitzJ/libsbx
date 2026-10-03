// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_SHAPES_HPP_
#define LIBSBX_PHYSICS_SHAPES_HPP_

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <variant>

#include <libsbx/math/constants.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/containers/static_vector.hpp>

#include <libsbx/utility/overload.hpp>

namespace sbx::physics {

/** @brief A sphere centered on its collider's origin. */
struct sphere {
  std::float_t radius{0.5f};
}; // struct sphere

/** @brief A capped cylinder along local +Y, centered on its collider's origin. */
struct cylinder {
  std::float_t radius{0.5f};
  std::float_t half_height{0.5f};
}; // struct cylinder

/** @brief A capsule along local +Y, centered on its collider's origin; half_height covers the cylinder only, not the caps. */
struct capsule {
  std::float_t radius{0.5f};
  std::float_t half_height{0.5f};
}; // struct capsule

/** @brief A local-space axis-aligned box centered on its collider's origin. */
struct box {
  math::vector3 half_extents{0.5f, 0.5f, 0.5f};
}; // struct box

/** @brief A single triangle, internal only: lets narrowphase treat mesh collider candidates uniformly. Never authored on a shape_collider. */
struct triangle {
  math::vector3 v0;
  math::vector3 v1;
  math::vector3 v2;
}; // struct triangle

/**
 * @brief The vertex budget for convex_hull. Hulls are exact unless a mesh's exact hull exceeds it, in which case it is coarsened.
 *
 * Also the largest convex_shape alternative, so it sets every convex_shape's size; a deliberate size/accuracy trade-off.
 */
inline constexpr auto convex_hull_max_points = std::size_t{64};

/** @brief Euler's bound (F = 2V - 4) for a fully triangulated hull, exact since quickhull never merges coplanar faces. */
inline constexpr auto convex_hull_max_faces = 2u * convex_hull_max_points - 4u;

struct convex_hull_face {
  std::array<std::uint16_t, 3> indices; // into convex_hull::points
}; // struct convex_hull_face

/**
 * @brief A view of a convex_hull_cache entry, which never moves once built. Internal only, for convex mesh colliders.
 *
 * `points` is all the support function needs, since a hull's furthest point is always a vertex; `faces` is only for debug drawing and may be empty for degenerate meshes.
 */
struct convex_hull {
  std::span<const math::vector3> points;
  std::span<const convex_hull_face> faces;
}; // struct convex_hull

using convex_shape = std::variant<sphere, cylinder, capsule, box, triangle, convex_hull>;

/**
 * @brief The GJK support function: the point on @p shape furthest along @p local_direction, in local space.
 *
 * @param shape The shape.
 * @param local_direction The search direction.
 *
 * @return The support point.
 */
[[nodiscard]] inline auto find_furthest_point(const convex_shape& shape, const math::vector3& local_direction) -> math::vector3 {
  return std::visit(utility::overload(
    [&](const sphere& shape) -> math::vector3 {
      const auto length = local_direction.length();

      if (length <= math::epsilonf) {
        return math::vector3{shape.radius, 0.0f, 0.0f};
      }

      return local_direction * (shape.radius / length);
    },
    [&](const cylinder& shape) -> math::vector3 {
      const auto lateral = math::vector3{local_direction.x(), 0.0f, local_direction.z()};
      const auto lateral_length = lateral.length();

      const auto xz = (lateral_length > math::epsilonf) ? lateral * (shape.radius / lateral_length) : math::vector3{shape.radius, 0.0f, 0.0f};
      const auto y = (local_direction.y() >= 0.0f) ? shape.half_height : -shape.half_height;

      return math::vector3{xz.x(), y, xz.z()};
    },
    [&](const capsule& shape) -> math::vector3 {
      const auto y_center = (local_direction.y() >= 0.0f) ? shape.half_height : -shape.half_height;
      const auto length = local_direction.length();

      const auto offset = (length > math::epsilonf) ? local_direction * (shape.radius / length) : math::vector3{shape.radius, 0.0f, 0.0f};

      return math::vector3{0.0f, y_center, 0.0f} + offset;
    },
    [&](const box& shape) -> math::vector3 {
      return math::vector3{
        (local_direction.x() >= 0.0f) ? shape.half_extents.x() : -shape.half_extents.x(),
        (local_direction.y() >= 0.0f) ? shape.half_extents.y() : -shape.half_extents.y(),
        (local_direction.z() >= 0.0f) ? shape.half_extents.z() : -shape.half_extents.z()
      };
    },
    [&](const triangle& shape) -> math::vector3 {
      const auto d0 = math::vector3::dot(shape.v0, local_direction);
      const auto d1 = math::vector3::dot(shape.v1, local_direction);
      const auto d2 = math::vector3::dot(shape.v2, local_direction);

      if (d0 >= d1 && d0 >= d2) {
        return shape.v0;
      }

      return (d1 >= d2) ? shape.v1 : shape.v2;
    },
    [&](const convex_hull& shape) -> math::vector3 {
      if (shape.points.empty()) {
        return math::vector3::zero;
      }

      auto best_point = shape.points[0];
      auto best_dot = math::vector3::dot(best_point, local_direction);

      for (auto index = std::size_t{1}; index < shape.points.size(); ++index) {
        const auto dot = math::vector3::dot(shape.points[index], local_direction);

        if (dot > best_dot) {
          best_dot = dot;
          best_point = shape.points[index];
        }
      }

      return best_point;
    }
  ), shape);
}

/**
 * @brief The shape's tight AABB in its own unrotated local frame.
 *
 * @param shape The shape.
 *
 * @return The bounds.
 */
[[nodiscard]] inline auto local_aabb(const convex_shape& shape) -> math::volume {
  return std::visit(utility::overload(
    [](const sphere& shape) -> math::volume {
      const auto extent = math::vector3{shape.radius, shape.radius, shape.radius};
      return math::volume{-extent, extent};
    },
    [](const cylinder& shape) -> math::volume {
      const auto extent = math::vector3{shape.radius, shape.half_height, shape.radius};
      return math::volume{-extent, extent};
    },
    [](const capsule& shape) -> math::volume {
      const auto half_full_height = shape.half_height + shape.radius;
      const auto extent = math::vector3{shape.radius, half_full_height, shape.radius};
      return math::volume{-extent, extent};
    },
    [](const box& shape) -> math::volume {
      return math::volume{-shape.half_extents, shape.half_extents};
    },
    [](const triangle& shape) -> math::volume {
      auto volume = math::volume{};
      volume.include(shape.v0);
      volume.include(shape.v1);
      volume.include(shape.v2);
      return volume;
    },
    [](const convex_hull& shape) -> math::volume {
      if (shape.points.empty()) {
        return math::volume{};
      }

      return math::volume::construct(shape.points);
    }
  ), shape);
}

/**
 * @brief The diagonal inverse inertia tensor in the shape's local frame, from standard solid-shape formulas; the capsule is a cylinder plus two hemispheres. Zero for triangles.
 *
 * @param shape The shape.
 * @param mass The mass.
 *
 * @return The diagonal inverse inertia.
 */
[[nodiscard]] inline auto local_inverse_inertia(const convex_shape& shape, std::float_t mass) -> math::vector3 {
  if (mass <= 0.0f) {
    return math::vector3::zero;
  }

  const auto invert_diagonal = [](const math::vector3& moments) -> math::vector3 {
    const auto safe_invert = [](std::float_t moment) -> std::float_t {
      return (moment > math::epsilonf) ? (1.0f / moment) : 0.0f;
    };

    return math::vector3{safe_invert(moments.x()), safe_invert(moments.y()), safe_invert(moments.z())};
  };

  return std::visit(utility::overload(
    [&](const sphere& shape) -> math::vector3 {
      const auto i = (2.0f / 5.0f) * mass * shape.radius * shape.radius;
      return invert_diagonal(math::vector3{i, i, i});
    },
    [&](const cylinder& shape) -> math::vector3 {
      const auto height = 2.0f * shape.half_height;
      const auto radius_squared = shape.radius * shape.radius;

      const auto i_y = 0.5f * mass * radius_squared;
      const auto i_x = mass * (3.0f * radius_squared + height * height) / 12.0f;

      return invert_diagonal(math::vector3{i_x, i_y, i_x});
    },
    [&](const capsule& shape) -> math::vector3 {
      const auto radius_squared = shape.radius * shape.radius;
      const auto height = 2.0f * shape.half_height;

      const auto cylinder_volume = std::numbers::pi_v<std::float_t> * radius_squared * height;
      const auto sphere_volume = (4.0f / 3.0f) * std::numbers::pi_v<std::float_t> * radius_squared * shape.radius;
      const auto total_volume = cylinder_volume + sphere_volume;

      if (total_volume <= math::epsilonf) {
        return math::vector3::zero;
      }

      // Cylinder plus two hemispheres, which together form one full sphere.
      const auto cylinder_mass = mass * cylinder_volume / total_volume;
      const auto caps_mass = mass * sphere_volume / total_volume;

      const auto i_y = 0.5f * cylinder_mass * radius_squared + 0.4f * caps_mass * radius_squared;

      // Each hemisphere's centroid sits 3r/8 from its flat face, for the parallel-axis term.
      const auto centroid_distance = shape.half_height + (3.0f / 8.0f) * shape.radius;

      const auto i_x =
        cylinder_mass * (3.0f * radius_squared + height * height) / 12.0f +
        caps_mass * ((83.0f / 320.0f) * radius_squared + centroid_distance * centroid_distance);

      return invert_diagonal(math::vector3{i_x, i_y, i_x});
    },
    [&](const box& shape) -> math::vector3 {
      const auto& half_extents = shape.half_extents;

      const auto i_x = (mass / 3.0f) * (half_extents.y() * half_extents.y() + half_extents.z() * half_extents.z());
      const auto i_y = (mass / 3.0f) * (half_extents.x() * half_extents.x() + half_extents.z() * half_extents.z());
      const auto i_z = (mass / 3.0f) * (half_extents.x() * half_extents.x() + half_extents.y() * half_extents.y());

      return invert_diagonal(math::vector3{i_x, i_y, i_z});
    },
    [&]([[maybe_unused]] const triangle& shape) -> math::vector3 {
      return math::vector3::zero;
    },
    [&](const convex_hull& shape) -> math::vector3 {
      // Approximated from the points' AABB with the box formula, assuming the hull is roughly centered (a v1 simplification).
      if (shape.points.empty()) {
        return math::vector3::zero;
      }

      const auto bounds = math::volume::construct(shape.points);
      const auto half_extents = bounds.extend() * 0.5f;

      const auto i_x = (mass / 3.0f) * (half_extents.y() * half_extents.y() + half_extents.z() * half_extents.z());
      const auto i_y = (mass / 3.0f) * (half_extents.x() * half_extents.x() + half_extents.z() * half_extents.z());
      const auto i_z = (mass / 3.0f) * (half_extents.x() * half_extents.x() + half_extents.y() * half_extents.y());

      return invert_diagonal(math::vector3{i_x, i_y, i_z});
    }
  ), shape);
}

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_SHAPES_HPP_
