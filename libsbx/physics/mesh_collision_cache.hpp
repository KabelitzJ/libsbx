// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PHYSICS_MESH_COLLISION_CACHE_HPP_
#define LIBSBX_PHYSICS_MESH_COLLISION_CACHE_HPP_

#include <cstdint>
#include <vector>

#include <libsbx/math/uuid.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/containers/dense_map.hpp>
#include <libsbx/containers/dynamic_tree.hpp>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::physics {

/** @brief A static mesh's triangle positions in mesh space plus a BVH whose leaf payload i is the triangle indices[3i..3i+2]. */
struct mesh_collision_data {
  std::vector<math::vector3> vertices;
  std::vector<std::uint32_t> indices;
  containers::dynamic_tree<std::uint32_t> triangle_bvh;
  math::volume local_bounds;
}; // struct mesh_collision_data

/** @brief Builds one mesh_collision_data per mesh uuid on first use from cooked data, independent of GPU residency, and never rebuilds. */
class mesh_collision_cache final : public utility::noncopyable {

public:

  [[nodiscard]] auto get_or_build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> const mesh_collision_data&;

  auto clear() -> void;

private:

  [[nodiscard]] auto _build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> mesh_collision_data;

  containers::dense_map<math::uuid, mesh_collision_data> _cache{};

}; // class mesh_collision_cache

} // namespace sbx::physics

#endif // LIBSBX_PHYSICS_MESH_COLLISION_CACHE_HPP_
