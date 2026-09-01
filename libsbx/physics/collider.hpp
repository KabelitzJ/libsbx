// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_COLLIDER_HPP_
#define LIBSBX_PHYSICS_COLLIDER_HPP_

#include <libsbx/math/matrix3x3.hpp>
#include <libsbx/math/quaternion.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/mesh.hpp>

#include <libsbx/physics/shapes.hpp>

namespace sbx::physics {

/**
 * @brief A single convex primitive collider. One per body in v1 — compound (multi-collider)
 * bodies are not supported.
 */
struct shape_collider {
  convex_shape shape{sphere{}};
  math::vector3 offset{math::vector3::zero};
  math::quaternion rotation{math::quaternion::identity};
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};
}; // struct shape_collider

/**
 * @brief A triangle-mesh collider. Mesh nodes carrying this must be authored without non-uniform
 * scale (their collision data is built once, in the mesh's own local space, and narrowphase assumes
 * a rotation+translation-only world transform).
 *
 * Unity MeshCollider parity: with `convex == false` (the default), the raw triangle mesh is used
 * directly and this can only be the non-simulated side of a contact -- valid on a static_body or a
 * kinematic body (e.g. a moving platform), never a dynamic_body (physics_module silently excludes
 * that combination from the broadphase, since there's no support-mapping for a concave shape).
 * With `convex == true`, a capped point-set approximation of the mesh's convex hull is used instead
 * (see convex_hull_cache.hpp) -- an ordinary convex_shape as far as narrowphase is concerned, so it
 * can be authored on any rigidbody type, dynamic included.
 */
struct mesh_collider {
  assets::mesh_handle mesh{};
  math::vector3 offset{math::vector3::zero};
  math::quaternion rotation{math::quaternion::identity};
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};
  bool is_convex{false};
}; // struct mesh_collider

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_COLLIDER_HPP_
