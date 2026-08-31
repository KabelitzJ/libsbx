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
 * @brief A static triangle-mesh collider. Mesh nodes carrying this must be authored without
 * non-uniform scale (their triangle data and BVH are built once, in the mesh's own local space,
 * and narrowphase assumes a rotation+translation-only world transform).
 */
struct mesh_collider {
  assets::mesh_handle mesh{};
  math::vector3 offset{math::vector3::zero};
  math::quaternion rotation{math::quaternion::identity};
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};
}; // struct mesh_collider

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_COLLIDER_HPP_
