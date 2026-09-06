// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_TERRAIN_TERRAIN_CHUNKING_HPP_
#define LIBSBX_TERRAIN_TERRAIN_CHUNKING_HPP_

#include <cstdint>
#include <vector>

#include <libsbx/math/vector2.hpp>
#include <libsbx/math/vector3.hpp>
#include <libsbx/math/vector4.hpp>
#include <libsbx/math/volume.hpp>

#include <libsbx/assets/mesh.hpp>
#include <libsbx/assets/material.hpp>

#include <libsbx/terrain/heightmap.hpp>

namespace sbx::terrain {

struct terrain_mesh_data {
  std::vector<assets::vertex> vertices{};
  std::vector<std::uint32_t> indices{};
  std::vector<assets::mesh::submesh> submeshes{};
  math::volume bounds{};
}; // struct terrain_mesh_data

/**
 * @brief Builds one mesh's worth of vertex/index/submesh data for the whole heightmap -- a single
 * chunk. Splitting a large heightmap into multiple, individually-culled chunks is a future
 * scalability step (asset_residency::create_mesh doesn't care either way); this covers the MVP
 * terrain sizes this system targets. Front-face winding matches the engine's counter_clockwise
 * convention (opaque_pass.cpp) as seen from above.
 */
[[nodiscard]] inline auto build_terrain_mesh(const heightmap& map, assets::material_handle material) -> terrain_mesh_data {
  auto data = terrain_mesh_data{};

  const auto width = map.width();
  const auto depth = map.depth();

  if (width < 2u || depth < 2u) {
    return data;
  }

  const auto extent = map.half_extent();
  const auto cell_size = map.cell_size();

  data.vertices.reserve(static_cast<std::size_t>(width) * depth);

  for (auto z = std::uint32_t{0}; z < depth; ++z) {
    for (auto x = std::uint32_t{0}; x < width; ++x) {
      const auto world_x = static_cast<std::float_t>(x) * cell_size - extent.x();
      const auto world_z = static_cast<std::float_t>(z) * cell_size - extent.y();
      const auto world_xz = math::vector2{world_x, world_z};

      const auto position = math::vector3{world_x, map.height_at(x, z), world_z};
      const auto normal = map.sample_normal(world_xz);
      const auto uv = math::vector2{
        static_cast<std::float_t>(x) / static_cast<std::float_t>(width - 1u),
        static_cast<std::float_t>(z) / static_cast<std::float_t>(depth - 1u)
      };

      data.vertices.push_back(assets::vertex{position, normal, uv, math::vector4{1.0f, 0.0f, 0.0f, 1.0f}});

      data.bounds.include(position);
    }
  }

  data.indices.reserve(static_cast<std::size_t>(width - 1u) * (depth - 1u) * 6u);

  for (auto z = std::uint32_t{0}; z < depth - 1u; ++z) {
    for (auto x = std::uint32_t{0}; x < width - 1u; ++x) {
      const auto i00 = z * width + x;
      const auto i10 = z * width + x + 1u;
      const auto i01 = (z + 1u) * width + x;
      const auto i11 = (z + 1u) * width + x + 1u;

      data.indices.push_back(i00);
      data.indices.push_back(i01);
      data.indices.push_back(i10);

      data.indices.push_back(i10);
      data.indices.push_back(i01);
      data.indices.push_back(i11);
    }
  }

  data.submeshes.push_back(assets::mesh::submesh{0u, static_cast<std::uint32_t>(data.indices.size()), data.bounds, material});

  return data;
}

} // namespace sbx::terrain

#endif // LIBSBX_TERRAIN_TERRAIN_CHUNKING_HPP_
