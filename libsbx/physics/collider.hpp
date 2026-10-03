// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_COLLIDER_HPP_
#define LIBSBX_PHYSICS_COLLIDER_HPP_

#include <memory>

#include <libsbx/math/matrix3x3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/mesh.hpp>

#include <libsbx/terrain/heightmap.hpp>

#include <libsbx/physics/shapes.hpp>

namespace sbx::physics {

/** @brief A convex primitive collider. A rigidbody uses every collider in its subtree (compound); without a rigidbody ancestor the collider is an implicit static body, like Unity. */
struct shape_collider {
  convex_shape shape{sphere{}};
  math::vector3 offset{math::vector3::zero};
  math::quaternion rotation{math::quaternion::identity};
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};

  // Triggers still generate contacts and fire contact events, but the solver never responds, like Unity's Is Trigger.
  bool is_trigger{false};
}; // struct shape_collider

/**
 * @brief A triangle mesh collider whose BVH or hull is built once in mesh space; scale is applied by the support mapping, so non-uniform scale works.
 *
 * Like Unity's MeshCollider: non-convex (the default) uses the raw mesh and is only valid on static or kinematic bodies; dynamic bodies carrying one are excluded.
 * Convex uses a capped hull approximation (convex_hull_cache.hpp) usable on any body type.
 */
struct mesh_collider {
  assets::mesh_handle mesh{};
  math::vector3 offset{math::vector3::zero};
  math::quaternion rotation{math::quaternion::identity};
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};
  bool is_convex{false};

  // See shape_collider::is_trigger.
  bool is_trigger{false};
}; // struct mesh_collider

/**
 * @brief A world-axis-aligned terrain heightfield collider, always an implicit static body.
 *
 * v1 only supports raycasts (raycast_heightfield), not contact generation.
 */
struct heightfield_collider {
  std::shared_ptr<const terrain::heightmap> data{};
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};
}; // struct heightfield_collider

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_COLLIDER_HPP_
