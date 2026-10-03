// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/narrowphase.hpp
 *
 * @brief Per-pair narrowphase: closed forms for sphere and capsule pairs, SAT with face clipping for box-box, and GJK/EPA for everything else, plus pose composition and compound collider resolution.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_NARROWPHASE_HPP_
#define LIBSBX_PHYSICS_NARROWPHASE_HPP_

#include <memory_resource>
#include <optional>
#include <unordered_map>
#include <vector>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/physics/contact.hpp>
#include <libsbx/physics/gjk.hpp>
#include <libsbx/physics/mesh_collision_cache.hpp>
#include <libsbx/physics/convex_hull_cache.hpp>

namespace sbx::physics {

// Per-step memo of compose_world_pose(), keyed by scenes::id. Cleared at the top of each step; composing a node also caches every ancestor it walked through.
using pose_cache = std::unordered_map<math::uuid, transform>;

/** @brief One convex primitive for narrowphase: its shape, full world pose (ancestors plus the collider's offset) and material. */
struct body_shape {
  convex_shape shape;
  transform pose;
  std::float_t friction{0.5f};
  std::float_t restitution{0.0f};
}; // struct body_shape

/**
 * @brief The node's live world pose, composing every ancestor's local_transform (with per-axis scale); world_transform is only refreshed once per frame. An unparented node costs one comparison.
 *
 * @param scene The scene.
 * @param node The node.
 * @param cache The step's pose cache.
 *
 * @return The world pose.
 */
[[nodiscard]] auto compose_world_pose(scenes::scene& scene, const scenes::node& node, pose_cache& cache) -> transform;

/**
 * @brief Folds a collider's own offset and rotation into its node's world pose, like a child transform.
 *
 * @param world_pose The node's world pose.
 * @param offset The collider's offset.
 * @param rotation The collider's rotation.
 *
 * @return The collider's world pose.
 */
[[nodiscard]] auto compose_pose(const transform& world_pose, const math::vector3& offset, const math::quaternion& rotation) -> transform;

/** @brief The node's own collider as one world-posed body_shape: a shape_collider or a convex mesh_collider's hull. nullopt for neither, a non-convex mesh collider, or an unresolvable one. */
[[nodiscard]] auto resolve_convex(scenes::scene& scene, const scenes::node& node, convex_hull_cache& hull_cache, assets::assets_module& assets_module, pose_cache& cache) -> std::optional<body_shape>;

/** @brief Every convex shape of a rigidbody: its own plus its descendants', stopping at descendants with their own rigidbody. @p resource lets callers keep the list on a stack arena. */
[[nodiscard]] auto resolve_body_shapes(scenes::scene& scene, const scenes::node& rigidbody_node, convex_hull_cache& hull_cache, assets::assets_module& assets_module, pose_cache& cache, std::pmr::memory_resource* resource = std::pmr::get_default_resource()) -> std::pmr::vector<body_shape>;

/**
 * @brief The nearest ancestor (inclusive) with a rigidbody, telling compound children apart from implicit-static colliders.
 *
 * @param scene The scene.
 * @param node The node to start from.
 *
 * @return The owning rigidbody's node, or nullopt.
 */
[[nodiscard]] auto find_owning_rigidbody(scenes::scene& scene, const scenes::node& node) -> std::optional<scenes::node>;

/** @brief Narrowphase for a candidate pair of bodies (compound rigidbodies or bare colliders), combining every shape-vs-shape (or shape-vs-triangle) touch into one manifold in the given node order. nullopt for two non-convex mesh colliders (like Unity) or no overlap. */
[[nodiscard]] auto generate_pair_contact(scenes::scene& scene, const sbx::scenes::node& node_a, const sbx::scenes::node& node_b, mesh_collision_cache& mesh_cache, convex_hull_cache& hull_cache, assets::assets_module& assets_module, pose_cache& cache) -> std::optional<contact_manifold>;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_NARROWPHASE_HPP_
