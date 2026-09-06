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

/**
 * @brief A single convex primitive collider. A rigidbody picks up every shape_collider/convex
 * mesh_collider in its own subtree, not just one on its exact same node (compound colliders -- see
 * narrowphase.hpp's resolve_body_shapes); a shape_collider with no rigidbody anywhere in its
 * ancestor chain is its own implicit-static body instead (matching Unity: a Collider alone, no
 * Rigidbody, is a static one).
 */
struct shape_collider {
  convex_shape shape{sphere{}};
  math::vector3 offset{math::vector3::zero};
  math::quaternion rotation{math::quaternion::identity};
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};

  // A trigger still generates contacts (broadphase/narrowphase run exactly as normal, and
  // physics_module::on_contact_began/on_contact_ended still fire for it -- see contact_manifold::
  // is_trigger's doc comment) but the solver never applies an impulse response for it, matching
  // Unity's "Is Trigger" checkbox.
  bool is_trigger{false};
}; // struct shape_collider

/**
 * @brief A triangle-mesh collider. Its collision data (a triangle BVH or convex hull) is built once,
 * in the mesh's own local space; narrowphase applies scale -- uniform or not -- as part of the
 * support mapping itself (see gjk.hpp's transform::scale doc comment) rather than warping the cached
 * geometry, so a non-uniform local_transform::scale works correctly here too.
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

  // See shape_collider::is_trigger's doc comment.
  bool is_trigger{false};
}; // struct mesh_collider

/**
 * @brief A terrain heightfield collider, world-axis-aligned (no offset/rotation -- a heightmap is
 * already defined in world space, unlike shape_collider/mesh_collider's node-local primitives).
 * Always an implicit-static body (see physics_module::_sync_broadphase): a heightfield never
 * carries a rigidbody, so a dynamic body can never be authored on top of one.
 *
 * v1 scope: participates only in physics_module::raycast() (picking), not narrowphase contact
 * generation -- a rigidbody resting/walking on procedural terrain needs a separate, larger piece
 * of work (heightfield-vs-convex-shape contact manifolds) not built yet. See raycast.hpp's
 * raycast_heightfield for the query this collider actually serves today.
 */
struct heightfield_collider {
  std::shared_ptr<const terrain::heightmap> data{};
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};
}; // struct heightfield_collider

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_COLLIDER_HPP_
