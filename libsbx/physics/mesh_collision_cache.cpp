// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/physics/mesh_collision_cache.hpp>

#include <cstring>
#include <string_view>
#include <tuple>
#include <vector>

#include <libsbx/utility/fourcc.hpp>
#include <libsbx/utility/hash.hpp>

#include <libsbx/physics/collision_cache_io.hpp>

namespace sbx::physics {

using tree_type = containers::dynamic_tree<std::uint32_t>;

constexpr auto cache_magic = utility::fourcc_v<"SBCM">; // 'SBCM' -- SBx Collision Mesh
constexpr auto cache_format_version = std::uint32_t{1};
constexpr auto cache_extension = std::string_view{".sbxcol"};

// What the cached BVH actually depends on: the mesh's vertex positions and its (LOD0, all
// submeshes concatenated) index list -- triangle connectivity affects the tree, so both matter,
// unlike convex_hull_cache which only ever looks at positions.
[[nodiscard]] auto compute_source_hash(const std::vector<math::vector3>& vertices, const std::vector<std::uint32_t>& indices) -> std::uint64_t {
  auto bytes = std::vector<std::uint8_t>{};
  bytes.resize(vertices.size() * sizeof(math::vector3) + indices.size() * sizeof(std::uint32_t));

  std::memcpy(bytes.data(), vertices.data(), vertices.size() * sizeof(math::vector3));
  std::memcpy(bytes.data() + vertices.size() * sizeof(math::vector3), indices.data(), indices.size() * sizeof(std::uint32_t));

  return utility::djb2_hash<std::uint64_t>{}(bytes);
}

// vertices/indices are never stored in the cache file: _build already has them for free every call
// (resolve_mesh_collision_data is itself disk-cached and cheap -- see collision_cache_io.hpp's file
// doc comment), so only the genuinely expensive-to-derive part -- the built BVH -- is persisted.
[[nodiscard]] auto try_read_disk_cache(const math::uuid& mesh_id, std::uint64_t source_hash, mesh_collision_data& data) -> bool {
  auto stream = open_collision_cache_for_read(collision_cache_path(cache_extension, mesh_id), cache_magic, cache_format_version, source_hash);

  if (!stream) {
    return false;
  }

  auto bounds_min = math::vector3{};
  auto bounds_max = math::vector3{};
  stream->read(reinterpret_cast<char*>(&bounds_min), sizeof(bounds_min));
  stream->read(reinterpret_cast<char*>(&bounds_max), sizeof(bounds_max));

  auto node_count = std::uint32_t{0};
  stream->read(reinterpret_cast<char*>(&node_count), sizeof(node_count));

  auto nodes = std::vector<tree_type::node>{};
  nodes.reserve(node_count);

  for (auto index = std::uint32_t{0}; index < node_count; ++index) {
    auto node = tree_type::node{};
    stream->read(reinterpret_cast<char*>(&node), sizeof(node));
    nodes.push_back(node);
  }

  auto root = tree_type::id{0};
  stream->read(reinterpret_cast<char*>(&root), sizeof(root));

  if (!*stream) {
    return false;
  }

  data.local_bounds = math::volume{bounds_min, bounds_max};
  data.triangle_bvh.rebuild(std::move(nodes), root);

  return true;
}

auto write_disk_cache(const math::uuid& mesh_id, const mesh_collision_data& data, std::uint64_t source_hash) -> void {
  auto stream = open_collision_cache_for_write(collision_cache_path(cache_extension, mesh_id), cache_magic, cache_format_version, source_hash);

  if (!stream) {
    return;
  }

  const auto bounds_min = data.local_bounds.min();
  const auto bounds_max = data.local_bounds.max();
  stream->write(reinterpret_cast<const char*>(&bounds_min), sizeof(bounds_min));
  stream->write(reinterpret_cast<const char*>(&bounds_max), sizeof(bounds_max));

  const auto node_count = static_cast<std::uint32_t>(data.triangle_bvh.node_count());
  stream->write(reinterpret_cast<const char*>(&node_count), sizeof(node_count));

  for (auto index = tree_type::id{0}; index < node_count; ++index) {
    const auto& node = data.triangle_bvh.node_at(index);
    stream->write(reinterpret_cast<const char*>(&node), sizeof(node));
  }

  const auto root = data.triangle_bvh.root_id();
  stream->write(reinterpret_cast<const char*>(&root), sizeof(root));
}

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

  const auto source_hash = compute_source_hash(data.vertices, data.indices);

  if (try_read_disk_cache(mesh_id, source_hash, data)) {
    return data;
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

  write_disk_cache(mesh_id, data, source_hash);

  return data;
}

} // namespace sbx::physics
