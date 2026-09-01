// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/narrowphase.hpp
 *
 * @brief Per-pair narrowphase dispatch: closed forms for the cheap/common pairs (sphere-*,
 * capsule-capsule), SAT with face clipping for box-box (the primary stacking/resting case), and
 * generic GJK/EPA for everything else.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_NARROWPHASE_HPP_
#define LIBSBX_PHYSICS_NARROWPHASE_HPP_

#include <optional>

#include <libsbx/scenes/node.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/physics/contact.hpp>
#include <libsbx/physics/mesh_collision_cache.hpp>
#include <libsbx/physics/convex_hull_cache.hpp>

namespace sbx::physics {

/**
 * @brief Runs narrowphase for a broadphase-candidate pair, both of which must carry a
 * shape_collider. Returns nullopt if the shapes don't actually overlap.
 */
[[nodiscard]] auto generate_contact(const sbx::scenes::node& node_a, const sbx::scenes::node& node_b) -> std::optional<contact_manifold>;

/**
 * @brief Runs narrowphase for any broadphase-candidate pair -- shape_collider vs shape_collider
 * (delegates to generate_contact), shape_collider vs a non-convex mesh_collider (per-triangle,
 * against its mesh_collision_cache BVH), or shape_collider/mesh_collider vs a convex mesh_collider
 * (its convex_hull_cache point set, an ordinary GJK/EPA call). Returns nullopt for a pair of two
 * mesh_colliders (never a supported pairing) or if either shape doesn't actually overlap. The
 * returned manifold's node_a/node_b always match the order they were passed in, regardless of which
 * side (if either) turned out to be the mesh.
 */
[[nodiscard]] auto generate_pair_contact(const sbx::scenes::node& node_a, const sbx::scenes::node& node_b, mesh_collision_cache& mesh_cache, convex_hull_cache& hull_cache, assets::assets_module& assets_module) -> std::optional<contact_manifold>;

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_NARROWPHASE_HPP_
