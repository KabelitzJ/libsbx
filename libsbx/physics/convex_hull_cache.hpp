// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz

/**
 * @file libsbx/physics/convex_hull_cache.hpp
 *
 * @brief Lazily built convex hulls per mesh uuid for convex mesh colliders, like mesh_collision_cache.
 *
 * @ingroup libsbx-physics
 */

#ifndef LIBSBX_PHYSICS_CONVEX_HULL_CACHE_HPP_
#define LIBSBX_PHYSICS_CONVEX_HULL_CACHE_HPP_

#include <memory>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/containers/dense_map.hpp>
#include <libsbx/containers/static_vector.hpp>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/physics/shapes.hpp>

namespace sbx::physics {

/** @brief A mesh's convex hull in mesh space, capped at convex_hull_max_points vertices and convex_hull_max_faces faces. */
struct convex_hull_data {
  containers::static_vector<math::vector3, convex_hull_max_points> points;
  containers::static_vector<convex_hull_face, convex_hull_max_faces> faces;
  math::volume local_bounds;
}; // struct convex_hull_data

/** @brief Builds one hull per mesh uuid on first use and never rebuilds; entries never move until clear(), so narrowphase's views stay valid. */
class convex_hull_cache final : public utility::noncopyable {

public:

  [[nodiscard]] auto get_or_build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> const convex_hull_data&;

  auto clear() -> void;

private:

  [[nodiscard]] auto _build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> convex_hull_data;

  containers::dense_map<math::uuid, std::unique_ptr<const convex_hull_data>> _cache{};

}; // class convex_hull_cache

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_CONVEX_HULL_CACHE_HPP_
