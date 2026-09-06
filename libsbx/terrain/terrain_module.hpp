// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_TERRAIN_TERRAIN_MODULE_HPP_
#define LIBSBX_TERRAIN_TERRAIN_MODULE_HPP_

#include <memory>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/physics/physics_module.hpp>

#include <libsbx/terrain/heightmap.hpp>
#include <libsbx/terrain/heightmap_generator.hpp>

namespace sbx::terrain {

/**
 * @brief Owns the active scene's terrain heightmap and exposes height/normal sampling for
 * anything that needs to conform to the ground -- road placement (via the C# Terrain.SampleHeight/
 * SampleNormal bindings), building placement, etc. generate() builds the heightmap, its combined
 * chunk mesh (assets::assets_module::create_mesh, rendered through the ordinary mesh_renderer/
 * opaque_pass path -- see heightmap.hpp/terrain_chunking.hpp's own doc comments for why terrain,
 * unlike roads, uses the real static mesh pipeline), and a heightfield_collider node for picking
 * (physics::physics_module::raycast).
 *
 * v1: one heightmap, one combined chunk mesh, generated procedurally on demand (no map-authoring/
 * import workflow yet -- see heightmap_generator.hpp).
 */
class terrain_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<assets::assets_module, scenes::scenes_module, physics::physics_module>;

  terrain_module() = default;

  /**
   * @brief (Re)generates the active scene's terrain from @p settings. Destroys the node from any
   * previous call before creating the new one, so calling this again replaces the terrain rather
   * than accumulating copies.
   */
  auto generate(const heightmap_generator_settings& settings = {}) -> void;

  [[nodiscard]] auto has_terrain() const noexcept -> bool {
    return _heightmap != nullptr;
  }

  [[nodiscard]] auto heightmap() const noexcept -> std::shared_ptr<const terrain::heightmap> {
    return _heightmap;
  }

  /** @brief 0 (and a warning) if generate() hasn't been called yet -- see heightmap::sample_bilinear's own empty-map fallback. */
  [[nodiscard]] auto sample_height(const math::vector2& world_xz) const -> std::float_t;

  /** @brief +Y (and a warning) if generate() hasn't been called yet -- see heightmap::sample_normal's own empty-map fallback. */
  [[nodiscard]] auto sample_normal(const math::vector2& world_xz) const -> math::vector3;

private:

  std::shared_ptr<terrain::heightmap> _heightmap{};
  scenes::node _terrain_node{};

}; // class terrain_module

} // namespace sbx::terrain

#endif // LIBSBX_TERRAIN_TERRAIN_MODULE_HPP_
