// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/convex_hull_cache.hpp
 *
 * @brief Lazily builds and caches a mesh's convex hull (vertices + triangle faces, via
 * quickhull.hpp), keyed by mesh uuid -- mirrors mesh_collision_cache.hpp's shape and lifecycle
 * exactly, but for mesh_collider::is_convex == true instead of the raw-triangle case.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_CONVEX_HULL_CACHE_HPP_
#define LIBSBX_PHYSICS_CONVEX_HULL_CACHE_HPP_

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/containers/dense_map.hpp>
#include <libsbx/containers/static_vector.hpp>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/physics/shapes.hpp>

namespace sbx::physics {

/**
 * @brief A mesh's convex hull, capped at convex_hull_max_points vertices / convex_hull_max_faces
 * faces, in the mesh's own local space (see convex_hull_cache::_build for how the cap is reached).
 */
struct convex_hull_data {
  containers::static_vector<math::vector3, convex_hull_max_points> points;
  containers::static_vector<convex_hull_face, convex_hull_max_faces> faces;
  math::volume local_bounds;
}; // struct convex_hull_data

/**
 * @brief Lazily builds and caches one convex_hull_data per mesh asset uuid. Built once, never
 * rebuilt -- same convention as mesh_collision_cache.
 */
class convex_hull_cache final : public utility::noncopyable {

public:

  [[nodiscard]] auto get_or_build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> const convex_hull_data&;

  auto clear() -> void;

private:

  [[nodiscard]] auto _build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> convex_hull_data;

  containers::dense_map<math::uuid, convex_hull_data> _cache{};

}; // class convex_hull_cache

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_CONVEX_HULL_CACHE_HPP_
