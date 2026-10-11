// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/terrain/terrain_module.hpp>

#include <memory>
#include <utility>
#include <vector>

#include <libsbx/math/color.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/physics/collider.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/terrain/terrain_chunking.hpp>

#include <stb_image.h>

namespace sbx::terrain {

auto terrain_module::generate(const heightmap_generator_settings& settings) -> void {
  _rebuild(std::make_shared<terrain::heightmap>(generate_heightmap(settings)));

  utility::logger<"terrain">::info("Generated terrain: {}x{} @ {} units/cell", settings.width, settings.depth, settings.cell_size);
}

auto terrain_module::load(const std::filesystem::path& path, std::float_t cell_size, std::float_t height_scale) -> bool {
  const auto resolved = core::engine::project().assets_directory() / path;

  auto width = 0;
  auto depth = 0;
  auto channels = 0;

  const auto pixels = std::unique_ptr<stbi_us, decltype(&stbi_image_free)>{stbi_load_16(resolved.string().c_str(), &width, &depth, &channels, 1), &stbi_image_free};

  if (!pixels || width < 2 || depth < 2) {
    utility::logger<"terrain">::error("Could not load heightmap '{}': {}", resolved.generic_string(), stbi_failure_reason() ? stbi_failure_reason() : "too small");
    return false;
  }

  auto heights = std::vector<std::float_t>(static_cast<std::size_t>(width) * static_cast<std::size_t>(depth));

  for (auto i = std::size_t{0u}; i < heights.size(); ++i) {
    heights[i] = static_cast<std::float_t>(pixels.get()[i]) / 65535.0f * height_scale;
  }

  _rebuild(std::make_shared<terrain::heightmap>(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(depth), cell_size, std::move(heights)));

  utility::logger<"terrain">::info("Loaded terrain '{}': {}x{} @ {} units/cell", path.generic_string(), width, depth, cell_size);

  return true;
}

auto terrain_module::_rebuild(std::shared_ptr<terrain::heightmap> map) -> void {
  _heightmap = std::move(map);

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
