// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/terrain/terrain_module.hpp>

#include <utility>
#include <vector>

#include <libsbx/math/color.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/physics/collider.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/terrain/terrain_chunking.hpp>

namespace sbx::terrain {

auto terrain_module::generate(const heightmap_generator_settings& settings) -> void {
  _heightmap = std::make_shared<terrain::heightmap>(generate_heightmap(settings));

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  if (_terrain_node.is_valid()) {
    scene.destroy_node(_terrain_node);
  }

  const auto material = assets_module.create_material(assets::material::create_info{
    .name = "Terrain",
    .base_color_factor = math::color{0.35f, 0.5f, 0.25f, 1.0f},
    .metallic_factor = 0.0f,
    .roughness_factor = 0.9f
  });

  const auto mesh_data = build_terrain_mesh(*_heightmap, material);
  const auto mesh = assets_module.create_mesh(mesh_data.vertices, mesh_data.indices, mesh_data.submeshes, mesh_data.bounds);

  _terrain_node = scene.create_node("Terrain");
  _terrain_node.add_component<scenes::mesh_renderer>(scenes::mesh_renderer{mesh, std::vector<assets::material_handle>{material}});
  _terrain_node.add_component<physics::heightfield_collider>(physics::heightfield_collider{_heightmap});

  utility::logger<"terrain">::info("Generated terrain: {}x{} @ {} units/cell", settings.width, settings.depth, settings.cell_size);
}

auto terrain_module::sample_height(const math::vector2& world_xz) const -> std::float_t {
  if (!_heightmap) {
    utility::logger<"terrain">::warn("sample_height called before generate() -- returning 0");
    return 0.0f;
  }

  return _heightmap->sample_bilinear(world_xz);
}

auto terrain_module::sample_normal(const math::vector2& world_xz) const -> math::vector3 {
  if (!_heightmap) {
    utility::logger<"terrain">::warn("sample_normal called before generate() -- returning +Y");
    return math::vector3{0.0f, 1.0f, 0.0f};
  }

  return _heightmap->sample_normal(world_xz);
}

} // namespace sbx::terrain
