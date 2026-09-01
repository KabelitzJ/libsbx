// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/narrowphase.hpp
 *
 * @brief Per-pair narrowphase dispatch: closed forms for the cheap/common pairs (sphere-*,
 * capsule-capsule), SAT with face clipping for box-box (the primary stacking/resting case), and
 * generic GJK/EPA for everything else -- plus the pose-composition and shape-resolution machinery
 * that lets a rigidbody's colliders live anywhere in its subtree (compound colliders), a collider
 * live with no rigidbody at all (implicit-static), and either be authored under a non-1
 * local_transform::scale -- uniform or not.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_NARROWPHASE_HPP_
#define LIBSBX_PHYSICS_NARROWPHASE_HPP_

#include <optional>
#include <vector>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/physics/contact.hpp>
#include <libsbx/physics/gjk.hpp>
#include <libsbx/physics/mesh_collision_cache.hpp>
#include <libsbx/physics/convex_hull_cache.hpp>

namespace sbx::physics {

/**
 * @brief One convex primitive resolved for narrowphase: its shape, full world pose (every
 * ancestor's local_transform composed together, see compose_world_pose, plus the collider's own
 * offset/rotation), and material.
 */
struct body_shape {
  convex_shape shape;
  transform pose;
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};
}; // struct body_shape

/**
 * @brief The live equivalent of what world_transform would give @p node if it weren't only
 * recomputed once per frame (see physics_module's class doc comment for why physics can't use that
 * cached copy). Walks node's parent chain up to the scene's true root, composing
 * local_transform::position/rotation/scale (a full per-axis scale -- see the doc comment on
 * physics::transform::scale) at each level on the way back down. Degenerates to node's own
 * local_transform, verbatim, for an ordinary unparented node -- the common case pays only the cost
 * of one parent-pointer comparison.
 */
[[nodiscard]] auto compose_world_pose(scenes::scene& scene, const scenes::node& node) -> transform;

/**
 * @brief Folds a collider's own offset/rotation (authored relative to @p world_pose, e.g.
 * shape_collider::offset) into @p world_pose -- the two are combined the same way any child
 * local_transform folds into its parent's world pose.
 */
[[nodiscard]] auto compose_pose(const transform& world_pose, const math::vector3& offset, const math::quaternion& rotation) -> transform;

/**
 * @brief Resolves @p node's own collider -- a shape_collider, or a mesh_collider with is_convex ==
 * true (via its cached hull point set; from here on indistinguishable to dispatch()/GJK/EPA from an
 * ordinary shape_collider) -- into a single body_shape, world-posed via compose_world_pose. Returns
 * nullopt if @p node carries neither, or a non-convex mesh_collider (it can never stand in as a
 * single convex_shape -- see generate_mesh_contact), or an unresolvable one (no mesh assigned, or an
 * empty cached hull).
 */
[[nodiscard]] auto resolve_convex(scenes::scene& scene, const scenes::node& node, convex_hull_cache& hull_cache, assets::assets_module& assets_module) -> std::optional<body_shape>;

/**
 * @brief Resolves every convex primitive owned by @p rigidbody_node: itself (resolve_convex) plus
 * every descendant's, recursing through the subtree but never past a descendant that has its own
 * rigidbody -- that one is an independent body instead, not part of this compound. A childless node
 * returns 0 or 1 entries with no recursion overhead, so an ordinary single-shape body costs exactly
 * what it always did.
 */
[[nodiscard]] auto resolve_body_shapes(scenes::scene& scene, const scenes::node& rigidbody_node, convex_hull_cache& hull_cache, assets::assets_module& assets_module) -> std::vector<body_shape>;

/**
 * @brief Walks upward from @p node (inclusive) through relationship::parent until it finds an
 * ancestor with a rigidbody, or reaches the scene root without finding one. Used to tell a bare
 * shape_collider/mesh_collider node apart from one that's really a compound child of some ancestor's
 * rigidbody -- the latter must not also be treated as its own independent implicit-static body.
 */
[[nodiscard]] auto find_owning_rigidbody(scenes::scene& scene, const scenes::node& node) -> std::optional<scenes::node>;

/**
 * @brief Runs narrowphase for a broadphase-candidate pair of bodies -- each side a rigidbody
 * (possibly compound, resolve_body_shapes) or a bare implicit-static collider node. Cross-tests
 * every shape on one side against every shape (or, when the other side is a non-convex
 * mesh_collider, every candidate BVH triangle) on the other, combining every touch into a single
 * contact_manifold for the pair. Returns nullopt for a pair of two non-convex mesh_colliders (never
 * a supported pairing, matching Unity) or if nothing on either side actually overlaps. The returned
 * manifold's node_a/node_b always match the order @p node_a/@p node_b were passed in.
 */
[[nodiscard]] auto generate_pair_contact(scenes::scene& scene, const sbx::scenes::node& node_a, const sbx::scenes::node& node_b, mesh_collision_cache& mesh_cache, convex_hull_cache& hull_cache, assets::assets_module& assets_module) -> std::optional<contact_manifold>;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_NARROWPHASE_HPP_
