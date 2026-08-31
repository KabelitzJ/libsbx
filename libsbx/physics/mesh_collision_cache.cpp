// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/mesh_collision_cache.hpp>

#include <tuple>

namespace sbx::physics {

auto mesh_collision_cache::get_or_build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> const mesh_collision_data& {
  if (!_cache.contains(mesh_id)) {
    _cache.emplace(mesh_id, _build(assets_module, mesh_id));
  }

  return _cache.at(mesh_id);
}

auto mesh_collision_cache::clear() -> void {
  _cache.clear();
}

auto mesh_collision_cache::_build(assets::assets_module& assets_module, const math::uuid& mesh_id) -> mesh_collision_data {
  auto data = mesh_collision_data{};

  const auto cooked = assets_module.resolve_mesh_collision_data(mesh_id);

  if (!cooked) {
    return data;
  }

  data.vertices.reserve(cooked->vertices.size());

  for (const auto& vertex : cooked->vertices) {
    data.vertices.push_back(vertex.position);
  }

  // Concatenate every submesh's LOD0 index range — full precision for collision, LOD chains
  // (mesh_lod) are a render-only concept and are ignored here.
  for (const auto& submesh : cooked->submeshes) {
    const auto begin = submesh.index_offset;
    const auto end = submesh.index_offset + submesh.index_count;

    for (auto index = begin; index < end; ++index) {
      data.indices.push_back(cooked->indices[index]);
    }
  }

  const auto triangle_count = static_cast<std::uint32_t>(data.indices.size() / 3u);

  for (auto triangle_index = std::uint32_t{0}; triangle_index < triangle_count; ++triangle_index) {
    const auto i0 = data.indices[triangle_index * 3u + 0u];
    const auto i1 = data.indices[triangle_index * 3u + 1u];
    const auto i2 = data.indices[triangle_index * 3u + 2u];

    auto bounds = math::volume{};
    bounds.include(data.vertices[i0]);
    bounds.include(data.vertices[i1]);
    bounds.include(data.vertices[i2]);

    data.local_bounds.include(bounds);
    [[maybe_unused]] const auto id = data.triangle_bvh.insert(triangle_index, bounds);
  }

  return data;
}

} // namespace sbx::physics
