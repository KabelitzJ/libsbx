// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_SURFACE_RAYCAST_HPP_
#define EDITOR_SURFACE_RAYCAST_HPP_

#include <optional>
#include <span>

#include <libsbx/math/ray.hpp>
#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/mesh.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/physics/mesh_collision_cache.hpp>

namespace editor {

/** @brief Where an Edit-mode ray met the scene's visible surfaces. */
struct surface_hit {
  sbx::scenes::node node{};
  sbx::math::vector3 point{};
  sbx::math::vector3 normal{0.0f, 1.0f, 0.0f};
  std::float_t distance{0.0f};
}; // struct surface_hit

/**
 * @brief The nearest render-mesh triangle or heightfield a ray hits, for placing things in Edit mode.
 *
 * Physics only syncs its world while simulating and most props have no collider, so this tests the render meshes' cooked triangles (through an editor-owned BVH cache) and heightfields directly.
 *
 * @param scene The scene.
 * @param ray The ray, in world space.
 * @param ignored Nodes whose subtrees don't count, e.g. the nodes being placed.
 *
 * @return The hit, or nullopt.
 */
[[nodiscard]] auto raycast_surface(sbx::scenes::scene& scene, const sbx::math::ray& ray, std::span<const sbx::math::uuid> ignored = {}) -> std::optional<surface_hit>;

/**
 * @brief The editor's triangle data for a mesh asset, built on first use and rebuilt after the mesh is reimported (its generation moves); empty for meshes without cooked data (e.g. script-built ones).
 *
 * @param mesh The mesh.
 *
 * @return The data.
 */
[[nodiscard]] auto surface_mesh_data(const sbx::assets::mesh_handle& mesh) -> const sbx::physics::mesh_collision_data&;

} // namespace editor

#endif // EDITOR_SURFACE_RAYCAST_HPP_
