#ifndef DEMO_VEGETATION_HPP_
#define DEMO_VEGETATION_HPP_

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>

#include <libsbx/math/noise.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/scenes.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>

#include <libsbx/models/models.hpp>

#include <demo/terrain.hpp>
#include <demo/grid.hpp>

namespace demo {

struct tree_variant {
  sbx::math::uuid mesh_id;
  std::vector<sbx::scenes::static_mesh::submesh> submeshes;
}; // struct tree_variant

struct vegetation_params {
  float density_scale{0.008f};
  float density_threshold{0.2f};
  float tree_spacing{3.0f};
  float jitter{0.8f};
  float min_scale{0.7f};
  float max_scale{1.3f};
  float max_slope{2.0f};
  float min_height{1.0f};
  float seed{500.0f};
}; // struct vegetation_params

auto make_tree_variant(sbx::scenes::scene& scene, const std::string& mesh_name, const std::string& gltf_path) -> tree_variant {
  auto mesh_id = scene.get_mesh(mesh_name);
  auto materials = sbx::models::load_materials(gltf_path);

  return tree_variant{mesh_id, std::move(materials)};
}

auto generate_vegetation( sbx::scenes::scene& scene, const terrain& terr, const grid& grid, const std::vector<tree_variant>& variants, const vegetation_params& params = {}) -> std::vector<sbx::scenes::node> {
  auto tree_nodes = std::vector<sbx::scenes::node>{};

  if (variants.empty()) {
    return tree_nodes;
  }

  auto verts_w = terr.verts_w();
  auto verts_h = terr.verts_h();
  auto grid_w = verts_w - 1;
  auto grid_h = verts_h - 1;

  auto offset_x = static_cast<float>(grid_w) * grid::cell_size * 0.5f;
  auto offset_z = static_cast<float>(grid_h) * grid::cell_size * 0.5f;

  auto step = static_cast<std::uint32_t>(std::max(1.0f, params.tree_spacing));

  auto variant_count = static_cast<std::uint8_t>(variants.size());
  auto tree_index = 0u;

  auto trees = scene.create_node("Trees");

  for (auto cy = 0u; cy < grid_h; cy += step) {
    for (auto cx = 0u; cx < grid_w; cx += step) {

      // Skip occupied or road cells
      if (grid.at(cx, cy).building_id != 0) {
        continue;
      }

      if (grid.at(cx, cy).flags & 0x01) {
        continue;
      }

      auto wx = static_cast<float>(cx);
      auto wz = static_cast<float>(cy);

      // Forest density — large scale clusters
      auto density = sbx::math::noise::fractal(wx * params.density_scale + params.seed, wz * params.density_scale + params.seed, 3u);

      // Detail variation within clusters
      auto detail = sbx::math::noise::simplex(wx * params.density_scale * 4.0f + params.seed + 1000.0f, wz * params.density_scale * 4.0f + params.seed + 1000.0f);

      auto forest_value = density * 0.7f + detail * 0.3f;

      if (forest_value < params.density_threshold) {
        continue;
      }

      // Slope check — no trees on cliffs
      auto h00 = terr.get_height(cx, cy);
      auto h10 = terr.get_height(cx + 1, cy);
      auto h01 = terr.get_height(cx, cy + 1);
      auto h11 = terr.get_height(cx + 1, cy + 1);

      auto slope = std::max({std::abs(h00 - h10), std::abs(h00 - h01), std::abs(h10 - h11), std::abs(h01 - h11)});

      if (slope > params.max_slope) {
        continue;
      }

      // Height check — no trees below water
      auto avg_height = (h00 + h10 + h01 + h11) * 0.25f;

      if (avg_height < params.min_height) {
        continue;
      }

      // Deterministic jitter from noise
      auto jx = sbx::math::noise::simplex(wx * 7.13f + params.seed, wz * 7.13f) * params.jitter;
      auto jz = sbx::math::noise::simplex(wx * 11.37f, wz * 11.37f + params.seed) * params.jitter;

      auto local_x = (wx + 0.5f + jx) * grid::cell_size;
      auto local_z = (wz + 0.5f + jz) * grid::cell_size;

      auto world_x = local_x - offset_x;
      auto world_z = local_z - offset_z;
      auto world_y = terr.get_height_at(local_x, local_z);

      // Scale — denser forest cores get taller trees
      auto density_factor = (forest_value - params.density_threshold) / (1.0f - params.density_threshold);

      auto scale_noise = sbx::math::noise::simplex(wx * 3.7f + params.seed, wz * 3.7f + params.seed);

      auto scale = std::lerp(params.min_scale, params.max_scale, std::clamp(density_factor * 0.5f + scale_noise * 0.5f + 0.5f, 0.0f, 1.0f));

      // Rotation — deterministic per position
      auto rotation = sbx::math::noise::simplex(wx * 5.3f + params.seed, wz * 5.3f);

      auto rotation_deg = sbx::math::degree{rotation * 180.0f};

      // Variant selection — deterministic per position
      auto variant_noise = sbx::math::noise::simplex(wx * 13.7f, wz * 13.7f + params.seed);

      auto variant_idx = static_cast<std::size_t>(std::abs(variant_noise) * static_cast<float>(variant_count)) % variant_count;

      const auto& variant = variants[variant_idx];

      // Create scene node
      auto node = scene.create_child_node(trees, fmt::format("Tree_{}", tree_index++));

      scene.add_component<sbx::scenes::static_mesh>(node, variant.mesh_id, variant.submeshes);

      auto& transform = scene.get_component<sbx::scenes::transform>(node);
      transform.set_position(sbx::math::vector3{world_x, world_y, world_z});
      transform.set_scale(sbx::math::vector3{scale, scale, scale});
      transform.set_rotation(sbx::math::vector3::up, rotation_deg);

      tree_nodes.push_back(node);
    }
  }

  return tree_nodes;
}

auto remove_trees_in_area(sbx::scenes::scene& scene, std::vector<sbx::scenes::node>& tree_nodes, float min_x, float min_z, float max_x, float max_z) -> void {
  std::erase_if(tree_nodes, [&](auto node) {
    auto& transform = scene.get_component<sbx::scenes::transform>(node);
    auto pos = transform.position();

    if (pos.x() >= min_x && pos.x() <= max_x && pos.z() >= min_z && pos.z() <= max_z) {
      scene.destroy_node(node);
      return true;
    }

    return false;
  });
}

} // namespace demo

#endif // DEMO_VEGETATION_HPP_