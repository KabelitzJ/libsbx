// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <system_error>
#include <unordered_map>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/math.hpp>

#include <meshoptimizer.h>

#include <libsbx/utility/fourcc.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/math/matrix_cast.hpp>

#include <libsbx/core/engine.hpp>

namespace sbx::assets {

inline constexpr auto mesh_magic = utility::fourcc_v<"SBSH">;   // 'SBSH'

struct mesh_file_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::uint32_t vertex_count;      // logical count after decoding vertex_data
  std::uint32_t index_count;       // logical count after decoding index_data (spans every submesh's LOD chain, not just LOD0)
  std::uint32_t submesh_count;
  std::float_t bounds_min[3];
  std::float_t bounds_max[3];
  std::uint32_t vertex_data_size;  // bytes of meshopt-encoded vertex buffer following the header
  std::uint32_t index_data_size;   // bytes of meshopt-encoded index buffer following the vertex data
  std::uint32_t flags;             // bit 0 = has_skin_data
  std::uint32_t skin_vertex_data_size; // bytes of *raw* (unencoded) skin_vertex array following the index data; 0 when unskinned
  std::uint32_t animation_clip_count;  // how many uint32 original-gltf-animation-index entries immediately follow the submesh records -- each resolvable via derive_animation_clip_uuid(id, that_index)
}; // struct mesh_file_header

inline constexpr auto mesh_flag_has_skin_data = std::uint32_t{1u << 0u};

struct submesh_file_record {
  std::uint32_t index_offset;
  std::uint32_t index_count;
  std::float_t bounds_min[3];
  std::float_t bounds_max[3];
  std::uint64_t material_uuid; // 0 = none
  std::uint32_t lod_count;     // submesh_lod_record entries immediately following this record
}; // struct submesh_file_record

struct submesh_lod_record {
  std::uint32_t index_offset;
  std::uint32_t index_count;
  std::float_t error;
}; // struct submesh_lod_record

auto asset_cooker::resolve_mesh(const std::filesystem::path& source, const math::uuid& id, const std::filesystem::path& cooked, const mesh_import_options& options, bool needs_cook, bool& did_cook) -> std::optional<cooked_mesh_data> {
  did_cook = false;

  if (needs_cook) {
    if (!_cook_mesh(source, id, cooked, options)) {
      return std::nullopt;
    }
    did_cook = true;
  }

  auto data = cooked_mesh_data{};
  auto animation_clip_original_indices = std::vector<std::uint32_t>{};

  if (!_load_cooked_mesh(cooked, data.vertices, data.indices, data.submeshes, data.bounds, data.skin_vertices, animation_clip_original_indices)) {
    if (!_cook_mesh(source, id, cooked, options) || !_load_cooked_mesh(cooked, data.vertices, data.indices, data.submeshes, data.bounds, data.skin_vertices, animation_clip_original_indices)) {
      utility::logger<"assets">::warn("Could not load cooked mesh '{}'", cooked.generic_string());
      return std::nullopt;
    }
    did_cook = true;
  }

  if (data.vertices.empty() || data.indices.empty()) {
    utility::logger<"assets">::warn("Mesh '{}' has no drawable geometry", source.generic_string());
    return std::nullopt;
  }

  if (!data.skin_vertices.empty()) {
    data.skeleton = derive_skeleton_uuid(id);

    data.animation_clips.reserve(animation_clip_original_indices.size());

    for (const auto original_index : animation_clip_original_indices) {
      data.animation_clips.push_back(derive_animation_clip_uuid(id, original_index));
    }
  }

  return data;
}

auto asset_cooker::inspect_mesh_source(const std::filesystem::path& source) -> std::optional<mesh_source_summary> {
  auto data = fastgltf::GltfDataBuffer::FromPath(source);

  if (data.error() != fastgltf::Error::None) {
    utility::logger<"assets">::warn("Inspect: could not open mesh '{}'", source.generic_string());
    return std::nullopt;
  }

  auto parser = fastgltf::Parser{fastgltf::Extensions::KHR_materials_emissive_strength | fastgltf::Extensions::KHR_materials_ior};

  // Same Options _cook_mesh uses -- a different flag set could change what's visible in
  // gltf.scenes/gltf.meshes/gltf.animations, and the two must stay in lockstep for
  // mesh_import_options::included_primitives/included_animations' indices to mean the same thing
  // here as they do at cook time.
  auto loaded = parser.loadGltf(data.get(), source.parent_path(), fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices);

  if (loaded.error() != fastgltf::Error::None) {
    utility::logger<"assets">::warn("Inspect: could not parse mesh '{}'", source.generic_string());
    return std::nullopt;
  }

  auto& gltf = loaded.get();

  auto summary = mesh_source_summary{};

  const auto append_primitive_summaries = [&](const fastgltf::Mesh& gltf_mesh) {
    const auto mesh_name = gltf_mesh.name.empty() ? std::string{"Mesh"} : std::string{gltf_mesh.name.begin(), gltf_mesh.name.end()};

    for (auto primitive_index = std::size_t{0u}; primitive_index < gltf_mesh.primitives.size(); ++primitive_index) {
      summary.primitives.push_back(mesh_source_primitive_summary{mesh_name, primitive_index});
    }
  };

  // Same traversal shape as _cook_mesh's append() call sites -- an instanced mesh (referenced by
  // more than one node) is listed once per node, matching that cooking will also produce separate
  // submeshes for each instance (see mesh_source_summary's doc comment).
  if (!gltf.scenes.empty()) {
    const auto scene_index = gltf.defaultScene.value_or(std::size_t{0});

    fastgltf::iterateSceneNodes(gltf, scene_index, fastgltf::math::fmat4x4{}, [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4&) {
      if (!node.meshIndex.has_value()) {
        return;
      }

      append_primitive_summaries(gltf.meshes[node.meshIndex.value()]);
    });
  } else {
    for (const auto& gltf_mesh : gltf.meshes) {
      append_primitive_summaries(gltf_mesh);
    }
  }

  // Same "first skin referenced wins" detection _cook_mesh uses -- on/off is all this reports,
  // since this cooker never supports more than one skeleton per mesh either way.
  if (!gltf.scenes.empty()) {
    const auto scene_index = gltf.defaultScene.value_or(std::size_t{0});

    fastgltf::iterateSceneNodes(gltf, scene_index, fastgltf::math::fmat4x4{}, [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4&) {
      if (node.meshIndex.has_value() && node.skinIndex.has_value()) {
        summary.has_skeleton = true;
      }
    });
  }

  summary.animation_names.reserve(gltf.animations.size());

  for (const auto& gltf_animation : gltf.animations) {
    summary.animation_names.push_back(gltf_animation.name.empty() ? std::string{"(unnamed)"} : std::string{gltf_animation.name.begin(), gltf_animation.name.end()});
  }

  return summary;
}

auto asset_cooker::gltf_external_file_references(const std::filesystem::path& source) -> std::vector<std::filesystem::path> {
  auto references = std::vector<std::filesystem::path>{};

  if (source.extension() != ".gltf") {
    return references; // .glb is one self-contained binary blob -- nothing external to find
  }

  auto data = fastgltf::GltfDataBuffer::FromPath(source);

  if (data.error() != fastgltf::Error::None) {
    return references;
  }

  auto parser = fastgltf::Parser{fastgltf::Extensions::KHR_materials_emissive_strength | fastgltf::Extensions::KHR_materials_ior};

  // Options::None -- unlike inspect_mesh_source/_cook_mesh, this only ever wants the raw uri
  // strings, never the referenced bytes, so there's nothing to gain from (and no reason to require
  // the files already existing for) an eager load.
  auto loaded = parser.loadGltf(data.get(), source.parent_path(), fastgltf::Options::None);

  if (loaded.error() != fastgltf::Error::None) {
    return references;
  }

  const auto& gltf = loaded.get();

  const auto collect = [&references](const auto& data_source) {
    if (const auto* uri = std::get_if<fastgltf::sources::URI>(&data_source)) {
      references.push_back(std::filesystem::path{std::string{uri->uri.path()}});
    }
  };

  for (const auto& buffer : gltf.buffers) {
    collect(buffer.data);
  }

  for (const auto& image : gltf.images) {
    collect(image.data);
  }

  return references;
}

auto asset_cooker::_generate_normals(std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count) -> void {
  // Area-weighted face-normal accumulation: a cross product's length is proportional to twice its
  // triangle's area, so summing it directly (before normalizing) naturally weights larger
  // triangles more, same idea as _generate_tangents' Lengyel accumulation below.
  auto normal_sum = std::vector<math::vector3>(vertex_count, math::vector3::zero);

  for (auto i = std::size_t{0u}; i + 2u < index_count; i += 3u) {
    const auto i0 = indices[index_start + i];
    const auto i1 = indices[index_start + i + 1u];
    const auto i2 = indices[index_start + i + 2u];

    const auto& v0 = vertices[i0];
    const auto& v1 = vertices[i1];
    const auto& v2 = vertices[i2];

    const auto face_normal = math::vector3::cross(v1.position - v0.position, v2.position - v0.position);

    for (const auto index : {i0, i1, i2}) {
      const auto local = index - static_cast<std::uint32_t>(vertex_start);
      normal_sum[local] = normal_sum[local] + face_normal;
    }
  }

  for (auto local = std::size_t{0u}; local < vertex_count; ++local) {
    auto& current = vertices[vertex_start + local];

    // Degenerate (isolated point / zero-area triangles only) falls back to a fixed up vector
    // rather than a zero normal -- NaN-ing every downstream lighting calculation is worse than a
    // wrong-but-finite normal on the handful of vertices this could ever affect.
    const auto normal = (normal_sum[local].length_squared() > 1e-12f) ? math::vector3::normalized(normal_sum[local]) : math::vector3{0.0f, 1.0f, 0.0f};

    current.normal[0] = normal.x();
    current.normal[1] = normal.y();
    current.normal[2] = normal.z();
  }
}

auto asset_cooker::_generate_tangents(std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count) -> void {
  // Lengyel's method: accumulate tangent/bitangent per vertex from referencing triangles, then
  // orthogonalize against the normal and derive handedness from the bitangent sum.
  auto tangent_sum = std::vector<math::vector3>(vertex_count, math::vector3::zero);
  auto bitangent_sum = std::vector<math::vector3>(vertex_count, math::vector3::zero);

  for (auto i = std::size_t{0u}; i + 2u < index_count; i += 3u) {
    const auto i0 = indices[index_start + i];
    const auto i1 = indices[index_start + i + 1u];
    const auto i2 = indices[index_start + i + 2u];

    const auto& v0 = vertices[i0];
    const auto& v1 = vertices[i1];
    const auto& v2 = vertices[i2];

    const auto edge1 = v1.position - v0.position;
    const auto edge2 = v2.position - v0.position;

    const auto delta_uv1 = v1.uv - v0.uv;
    const auto delta_uv2 = v2.uv - v0.uv;

    const auto denom = delta_uv1.x() * delta_uv2.y() - delta_uv2.x() * delta_uv1.y();
    const auto f = (std::abs(denom) > 1e-8f) ? (1.0f / denom) : 0.0f;

    const auto triangle_tangent = f * (edge1 * delta_uv2.y() - edge2 * delta_uv1.y());
    const auto triangle_bitangent = f * (edge2 * delta_uv1.x() - edge1 * delta_uv2.x());

    for (const auto index : {i0, i1, i2}) {
      const auto local = index - static_cast<std::uint32_t>(vertex_start);
      tangent_sum[local] = tangent_sum[local] + triangle_tangent;
      bitangent_sum[local] = bitangent_sum[local] + triangle_bitangent;
    }
  }

  for (auto local = std::size_t{0u}; local < vertex_count; ++local) {
    auto& current = vertices[vertex_start + local];

    const auto n = current.normal;
    auto t = tangent_sum[local] - n * math::vector3::dot(n, tangent_sum[local]);

    t = (t.length_squared() < 1e-12f) ? math::vector3::orthogonal(n) : math::vector3::normalized(t);

    const auto handedness = (math::vector3::dot(math::vector3::cross(n, t), bitangent_sum[local]) < 0.0f) ? -1.0f : 1.0f;

    current.tangent[0] = t.x();
    current.tangent[1] = t.y();
    current.tangent[2] = t.z();
    current.tangent[3] = handedness;
  }
}

auto asset_cooker::_optimize_and_generate_lods(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count, std::vector<skin_vertex>* skin_vertices) -> std::vector<mesh_lod> {
  auto lods = std::vector<mesh_lod>{};

  if (index_count == 0u || vertex_count == 0u) {
    return lods;
  }

  // meshopt works in a 0-based local index space, not indices' mesh-global one — translate this
  // submesh's slice down to local, optimize, then translate back before writing to the shared arrays.
  auto local = std::vector<std::uint32_t>(index_count);

  for (auto i = std::size_t{0u}; i < index_count; ++i) {
    local[i] = indices[index_start + i] - static_cast<std::uint32_t>(vertex_start);
  }

  // Standard GPU-friendly ordering trio: vertex cache (post-transform reuse), overdraw (front-to-back
  // triangle order), vertex fetch (pre-transform cache locality — reorders the vertex buffer itself).
  meshopt_optimizeVertexCache(local.data(), local.data(), index_count, vertex_count);
  meshopt_optimizeOverdraw(local.data(), local.data(), index_count, &vertices[vertex_start].position.x(), vertex_count, sizeof(vertex), 1.05f);

  // Computed as an explicit remap (rather than calling meshopt_optimizeVertexFetch directly) so the
  // same permutation can also be applied to skin_vertices -- a second, parallel vertex stream that
  // function has no way to know about (see its own doc comment on multiple vertex streams).
  auto remap = std::vector<unsigned int>(vertex_count);
  meshopt_optimizeVertexFetchRemap(remap.data(), local.data(), index_count, vertex_count);

  auto reordered = std::vector<vertex>(vertex_count);
  meshopt_remapVertexBuffer(reordered.data(), &vertices[vertex_start], vertex_count, sizeof(vertex), remap.data());
  std::ranges::copy(reordered, vertices.begin() + static_cast<std::ptrdiff_t>(vertex_start));

  if (skin_vertices != nullptr) {
    auto reordered_skin = std::vector<skin_vertex>(vertex_count);
    meshopt_remapVertexBuffer(reordered_skin.data(), &(*skin_vertices)[vertex_start], vertex_count, sizeof(skin_vertex), remap.data());
    std::ranges::copy(reordered_skin, skin_vertices->begin() + static_cast<std::ptrdiff_t>(vertex_start));
  }

  for (auto i = std::size_t{0u}; i < index_count; ++i) {
    local[i] = remap[local[i]];
    indices[index_start + i] = local[i] + static_cast<std::uint32_t>(vertex_start);
  }

  // Coarser LOD chain, each level targeting half the previous one's triangle budget; stops once
  // meshopt_simplify stalls (topology-locked) or the mesh is already too small to bother.
  auto previous = local;
  constexpr auto max_levels = std::size_t{4u};
  constexpr auto min_triangle_count = std::size_t{8u};

  for (auto level = std::size_t{0u}; level < max_levels; ++level) {
    const auto target_index_count = std::max((previous.size() / 2u) / 3u * 3u, min_triangle_count * 3u);

    if (target_index_count >= previous.size()) {
      break;
    }

    auto simplified = std::vector<std::uint32_t>(previous.size());
    auto result_error = 0.0f;

    const auto simplified_count = meshopt_simplify(
      simplified.data(), previous.data(), previous.size(),
      &vertices[vertex_start].position.x(), vertex_count, sizeof(vertex),
      target_index_count, 1e-2f, 0u, &result_error
    );

    // Less than ~10% reduction means the chain has bottomed out (topology constraints, etc).
    if (simplified_count == 0u || simplified_count >= (previous.size() * 9u) / 10u) {
      break;
    }

    simplified.resize(simplified_count);
    meshopt_optimizeVertexCache(simplified.data(), simplified.data(), simplified_count, vertex_count);

    const auto lod_offset = indices.size();
    indices.reserve(lod_offset + simplified_count);

    for (const auto index : simplified) {
      indices.push_back(index + static_cast<std::uint32_t>(vertex_start));
    }

    lods.push_back(mesh_lod{static_cast<std::uint32_t>(lod_offset), static_cast<std::uint32_t>(simplified_count), result_error});

    previous = std::move(simplified);
  }

  return lods;
}

auto asset_cooker::_cook_mesh(const std::filesystem::path& source, const math::uuid& id, const std::filesystem::path& cooked, const mesh_import_options& options) -> bool {
  auto data = fastgltf::GltfDataBuffer::FromPath(source);

  if (data.error() != fastgltf::Error::None) {
    utility::logger<"assets">::warn("Cook: could not open mesh '{}'", source.generic_string());
    return false;
  }

  auto parser = fastgltf::Parser{fastgltf::Extensions::KHR_materials_emissive_strength | fastgltf::Extensions::KHR_materials_ior};

  auto loaded = parser.loadGltf(data.get(), source.parent_path(), fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices);

  if (loaded.error() != fastgltf::Error::None) {
    utility::logger<"assets">::warn("Cook: could not parse mesh '{}'", source.generic_string());
    return false;
  }

  auto& gltf = loaded.get();

  // Referenced glTF images become a material_description texture-slot *path* (assets-directory-
  // relative), not a uuid -- resolving a path to a stable uuid is asset_manifest::import's job, and
  // asset_manifest is main-thread-only (see its own doc comment for why). asset_residency's mesh
  // finalize step turns this path into a handle via load_texture(path, ...) exactly the same way it
  // already does for a hand-authored `.material` file's texture slots.
  const auto& project = core::engine::project();

  const auto texture_path = [&](std::size_t texture_index) -> std::string {
    const auto& gltf_texture = gltf.textures[texture_index];

    if (!gltf_texture.imageIndex.has_value()) {
      return {};
    }

    const auto& image = gltf.images[gltf_texture.imageIndex.value()];

    if (const auto* uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
      const auto absolute = source.parent_path() / std::filesystem::path{std::string{uri->uri.path()}};
      return std::filesystem::relative(absolute, project.assets_directory()).generic_string();
    }

    utility::logger<"assets">::warn("Cook: mesh '{}' has a non-file image, using default", source.generic_string());
    return {};
  };

  auto material_uuids = std::vector<math::uuid>{};
  material_uuids.reserve(gltf.materials.size());

  for (const auto& gltf_material : gltf.materials) {
    const auto& pbr = gltf_material.pbrData;

    auto description = material_description{};
    description.name = gltf_material.name.empty() ? std::string{"material"} : std::string{gltf_material.name.begin(), gltf_material.name.end()};
    description.base_color_factor = math::color{pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]};
    description.emissive_factor = math::vector3{gltf_material.emissiveFactor[0], gltf_material.emissiveFactor[1], gltf_material.emissiveFactor[2]};
    description.metallic_factor = pbr.metallicFactor;
    description.roughness_factor = pbr.roughnessFactor;
    description.alpha = (gltf_material.alphaMode == fastgltf::AlphaMode::Blend) ? alpha_mode::blend : (gltf_material.alphaMode == fastgltf::AlphaMode::Mask) ? alpha_mode::mask : alpha_mode::opaque;
    description.alpha_cutoff = gltf_material.alphaCutoff;
    description.is_double_sided = gltf_material.doubleSided;
    description.normal_scale = gltf_material.normalTexture.has_value() ? gltf_material.normalTexture->scale : 1.0f;
    description.occlusion_strength = gltf_material.occlusionTexture.has_value() ? gltf_material.occlusionTexture->strength : 1.0f;
    description.emissive_strength = gltf_material.emissiveStrength;
    description.ior = gltf_material.ior;

    if (pbr.baseColorTexture.has_value())         description.albedo             = texture_path(pbr.baseColorTexture->textureIndex);
    if (pbr.metallicRoughnessTexture.has_value()) description.metallic_roughness = texture_path(pbr.metallicRoughnessTexture->textureIndex);
    if (gltf_material.normalTexture.has_value())  description.normal             = texture_path(gltf_material.normalTexture->textureIndex);
    if (gltf_material.occlusionTexture.has_value()) description.occlusion        = texture_path(gltf_material.occlusionTexture->textureIndex);
    if (gltf_material.emissiveTexture.has_value()) description.emissive          = texture_path(gltf_material.emissiveTexture->textureIndex);

    // Every embedded material is cooked as a self-contained, resolvable side-effect blob here --
    // same idea as a skinned mesh's skeleton/animation clips below. Whether this ends up being what
    // the submesh actually uses, or gets superseded by a hand-editable extracted `.material` file,
    // is decided later by asset_residency's main-thread mesh finalize step (mesh_import_options::
    // extract_materials) -- see cooked_submesh::material's doc comment.
    const auto material_uuid = derive_material_uuid(id, material_uuids.size());

    if (!_cook_material(material_uuid, description)) {
      return false;
    }

    material_uuids.push_back(material_uuid);
  }

  const auto material_uuid_for = [&](fastgltf::Optional<std::size_t> index) -> math::uuid {
    if (index.has_value() && index.value() < material_uuids.size()) {
      return material_uuids[index.value()];
    }

    return math::uuid::nil();
  };

  auto vertices = std::vector<vertex>{};
  auto skin_vertices = std::vector<skin_vertex>{};
  auto indices = std::vector<std::uint32_t>{};
  auto submeshes = std::vector<cooked_submesh>{};
  auto mesh_volume = math::volume{};

  // Skinning: one skeleton per cooked mesh -- the first skinned node's skin wins; any other skin
  // encountered later only warns. A node's world transform is *not* baked into a skinned
  // primitive's vertices (see the traversal below) -- glTF skinning requires vertices to stay in
  // the space the skin's inverse-bind matrices were authored against, with placement coming
  // entirely from the joint hierarchy instead.
  auto joints = std::vector<skeleton::joint>{};
  auto joint_remap = std::vector<std::uint32_t>{}; // skin-local (JOINTS_0) index -> joints' topologically-sorted index
  auto primary_skin_index = std::optional<std::size_t>{};
  auto node_to_joint = std::unordered_map<std::size_t, std::size_t>{}; // glTF node index -> joints index, for animation cooking below

  if (!gltf.scenes.empty()) {
    const auto scene_index = gltf.defaultScene.value_or(std::size_t{0});

    fastgltf::iterateSceneNodes(gltf, scene_index, fastgltf::math::fmat4x4{}, [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4&) {
      if (!node.meshIndex.has_value() || !node.skinIndex.has_value()) {
        return;
      }

      if (!primary_skin_index.has_value()) {
        primary_skin_index = node.skinIndex.value();
      } else if (*primary_skin_index != node.skinIndex.value()) {
        utility::logger<"assets">::warn("Cook: mesh '{}' references multiple skins; only the first is used", source.generic_string());
      }
    });
  }

  if (primary_skin_index.has_value()) {
    const auto& skin = gltf.skins[*primary_skin_index];
    const auto joint_count = skin.joints.size();

    auto node_to_skin_local = std::unordered_map<std::size_t, std::uint32_t>{};
    node_to_skin_local.reserve(joint_count);

    for (auto index = std::size_t{0u}; index < joint_count; ++index) {
      node_to_skin_local.emplace(skin.joints[index], static_cast<std::uint32_t>(index));
    }

    auto node_parent = std::unordered_map<std::size_t, std::size_t>{};

    for (auto node_index = std::size_t{0u}; node_index < gltf.nodes.size(); ++node_index) {
      for (const auto child : gltf.nodes[node_index].children) {
        node_parent.emplace(child, node_index);
      }
    }

    // Parent, in the *original* skin.joints order (still to be topo-sorted below); -1 if the
    // parent node isn't itself a joint of this skin (i.e. this is the skin's effective root).
    auto skin_local_parent = std::vector<std::int32_t>(joint_count, -1);

    for (auto index = std::size_t{0u}; index < joint_count; ++index) {
      if (const auto parent_entry = node_parent.find(skin.joints[index]); parent_entry != node_parent.end()) {
        if (const auto joint_entry = node_to_skin_local.find(parent_entry->second); joint_entry != node_to_skin_local.end()) {
          skin_local_parent[index] = static_cast<std::int32_t>(joint_entry->second);
        }
      }
    }

    // Topological sort by depth from root -- a joint's parent always has a strictly smaller
    // depth, so a stable sort on depth alone guarantees parent-before-child.
    auto depth = std::vector<std::uint32_t>(joint_count, 0u);

    for (auto index = std::size_t{0u}; index < joint_count; ++index) {
      auto current = skin_local_parent[index];

      while (current >= 0) {
        ++depth[index];
        current = skin_local_parent[static_cast<std::size_t>(current)];
      }
    }

    auto order = std::vector<std::uint32_t>(joint_count);
    std::iota(order.begin(), order.end(), std::uint32_t{0u});
    std::stable_sort(order.begin(), order.end(), [&](std::uint32_t a, std::uint32_t b) { return depth[a] < depth[b]; });

    joint_remap.resize(joint_count);

    for (auto new_index = std::uint32_t{0u}; new_index < joint_count; ++new_index) {
      joint_remap[order[new_index]] = new_index;
    }

    auto inverse_binds = std::vector<fastgltf::math::fmat4x4>(joint_count, fastgltf::math::fmat4x4{});

    if (skin.inverseBindMatrices.has_value()) {
      fastgltf::iterateAccessorWithIndex<fastgltf::math::fmat4x4>(gltf, gltf.accessors[skin.inverseBindMatrices.value()], [&](fastgltf::math::fmat4x4 value, std::size_t index) {
        inverse_binds[index] = value;
      });
    }

    joints.resize(joint_count);

    for (auto old_index = std::size_t{0u}; old_index < joint_count; ++old_index) {
      const auto new_index = joint_remap[old_index];
      const auto node_index = skin.joints[old_index];
      const auto& node = gltf.nodes[node_index];

      node_to_joint.emplace(node_index, new_index);

      auto& joint = joints[new_index];
      joint.name = node.name.empty() ? fmt::format("joint_{}", new_index) : std::string{node.name.begin(), node.name.end()};
      joint.parent_index = (skin_local_parent[old_index] < 0) ? -1 : static_cast<std::int32_t>(joint_remap[static_cast<std::size_t>(skin_local_parent[old_index])]);

      if (const auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
        joint.bind_local_translation = math::vector3{trs->translation.x(), trs->translation.y(), trs->translation.z()};
        joint.bind_local_rotation = math::quaternion::wxyz(trs->rotation.w(), trs->rotation.x(), trs->rotation.y(), trs->rotation.z());
        joint.bind_local_scale = math::vector3{trs->scale.x(), trs->scale.y(), trs->scale.z()};
      } else if (const auto* node_matrix = std::get_if<fastgltf::math::fmat4x4>(&node.transform)) {
        auto local = math::matrix4x4::identity;

        for (auto column = std::size_t{0u}; column < 4u; ++column) {
          for (auto row = std::size_t{0u}; row < 4u; ++row) {
            local[column][row] = (*node_matrix)[column][row];
          }
        }

        const auto decomposed = math::decompose(local);
        joint.bind_local_translation = decomposed.position;
        joint.bind_local_rotation = decomposed.rotation;
        joint.bind_local_scale = decomposed.scale;
      }

      const auto& inverse_bind = inverse_binds[old_index];

      for (auto column = std::size_t{0u}; column < 4u; ++column) {
        for (auto row = std::size_t{0u}; row < 4u; ++row) {
          joint.inverse_bind_matrix[column][row] = inverse_bind[column][row];
        }
      }
    }
  }

  const auto has_skin_data = options.import_skeleton && !joints.empty();

  // Shared across every append() call below (once per scene node, or once per gltf.meshes entry
  // for a scene-less file) -- the Nth primitive encountered overall is what
  // mesh_import_options::included_primitives/mesh_source_summary::primitives index by, so this
  // must increment exactly once per primitive regardless of which mesh/node it belongs to, matching
  // inspect_mesh_source's own traversal below.
  auto global_primitive_index = std::size_t{0u};

  const auto append = [&](const fastgltf::Mesh& gltf_mesh, const fastgltf::math::fmat4x4& world, bool is_skinned) {
    for (const auto& primitive : gltf_mesh.primitives) {
      const auto this_primitive_index = global_primitive_index++;

      if (!options.included_primitives.empty() && std::ranges::find(options.included_primitives, this_primitive_index) == options.included_primitives.end()) {
        continue;
      }

      const auto* position = primitive.findAttribute("POSITION");

      if (position == primitive.attributes.end()) {
        continue;
      }

      const auto vertex_start = vertices.size();

      const auto& position_accessor = gltf.accessors[position->accessorIndex];
      vertices.resize(vertex_start + position_accessor.count);

      // A mixed skinned/static file still keeps skin_vertices parallel to vertices for every
      // primitive -- entries a primitive doesn't overwrite below default to a rigid bind to
      // joints[0], a safe fallback for e.g. static decoration meshes sharing a skinned character file.
      if (has_skin_data) {
        skin_vertices.resize(vertex_start + position_accessor.count, skin_vertex{{0u, 0u, 0u, 0u}, math::vector4{1.0f, 0.0f, 0.0f, 0.0f}});
      }

      auto submesh_volume = math::volume{};

      fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, position_accessor, [&](fastgltf::math::fvec3 value, std::size_t index) {
        const auto world_position = world * fastgltf::math::fvec4{value[0], value[1], value[2], 1.0f};
        const auto point = math::vector3{world_position[0], world_position[1], world_position[2]};

        auto& current = vertices[vertex_start + index];
        current.position[0] = point.x();
        current.position[1] = point.y();
        current.position[2] = point.z();

        submesh_volume.include(point);
        mesh_volume.include(point);
      });

      const auto* normal = primitive.findAttribute("NORMAL");
      const auto has_explicit_normal = normal != primitive.attributes.end();

      if (has_explicit_normal) {
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normal->accessorIndex], [&](fastgltf::math::fvec3 value, std::size_t index) {
          const auto world_normal = world * fastgltf::math::fvec4{value[0], value[1], value[2], 0.0f};

          auto length = std::sqrt(world_normal[0] * world_normal[0] + world_normal[1] * world_normal[1] + world_normal[2] * world_normal[2]);
          length = (length > 0.0f) ? length : 1.0f;

          auto& current = vertices[vertex_start + index];
          current.normal[0] = world_normal[0] / length;
          current.normal[1] = world_normal[1] / length;
          current.normal[2] = world_normal[2] / length;
        });
      }

      const auto* tangent = primitive.findAttribute("TANGENT");
      const auto has_explicit_tangent = tangent != primitive.attributes.end();

      if (has_explicit_tangent) {
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[tangent->accessorIndex], [&](fastgltf::math::fvec4 value, std::size_t index) {
          const auto world_tangent = world * fastgltf::math::fvec4{value[0], value[1], value[2], 0.0f};

          auto length = std::sqrt(world_tangent[0] * world_tangent[0] + world_tangent[1] * world_tangent[1] + world_tangent[2] * world_tangent[2]);
          length = (length > 0.0f) ? length : 1.0f;

          auto& current = vertices[vertex_start + index];
          current.tangent[0] = world_tangent[0] / length;
          current.tangent[1] = world_tangent[1] / length;
          current.tangent[2] = world_tangent[2] / length;
          current.tangent[3] = value[3];
        });
      }

      if (const auto* uv = primitive.findAttribute("TEXCOORD_0"); uv != primitive.attributes.end()) {
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[uv->accessorIndex], [&](fastgltf::math::fvec2 value, std::size_t index) {
          vertices[vertex_start + index].uv[0] = value[0];
          vertices[vertex_start + index].uv[1] = value[1];
        });
      }

      if (is_skinned) {
        const auto* joints0 = primitive.findAttribute("JOINTS_0");
        const auto* weights0 = primitive.findAttribute("WEIGHTS_0");

        if (joints0 != primitive.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<fastgltf::math::u32vec4>(gltf, gltf.accessors[joints0->accessorIndex], [&](fastgltf::math::u32vec4 value, std::size_t index) {
            auto& current = skin_vertices[vertex_start + index];

            for (auto component = std::size_t{0u}; component < 4u; ++component) {
              const auto skin_local = value[component];
              current.joint_indices[component] = (skin_local < joint_remap.size()) ? joint_remap[skin_local] : std::uint32_t{0u};
            }
          });
        }

        if (weights0 != primitive.attributes.end()) {
          fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[weights0->accessorIndex], [&](fastgltf::math::fvec4 value, std::size_t index) {
            const auto sum = value[0] + value[1] + value[2] + value[3];
            const auto inverse_sum = (sum > 0.0f) ? (1.0f / sum) : 0.0f;

            skin_vertices[vertex_start + index].weights = math::vector4{value[0] * inverse_sum, value[1] * inverse_sum, value[2] * inverse_sum, value[3] * inverse_sum};
          });
        }
      }

      if (!primitive.indicesAccessor.has_value()) {
        continue;
      }

      const auto& index_accessor = gltf.accessors[primitive.indicesAccessor.value()];
      const auto index_start = indices.size();
      indices.reserve(index_start + index_accessor.count);

      fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor, [&](std::uint32_t index) {
        indices.push_back(static_cast<std::uint32_t>(vertex_start) + index);
      });

      // Positions are already in world space by this point (baked above, like the explicit-NORMAL
      // path already accounts for), so the generated face normals need no further transform.
      if (!has_explicit_normal) {
        _generate_normals(vertices, indices, vertex_start, position_accessor.count, index_start, index_accessor.count);
      }

      if (!has_explicit_tangent) {
        _generate_tangents(vertices, indices, vertex_start, position_accessor.count, index_start, index_accessor.count);
      }

      auto lods = _optimize_and_generate_lods(vertices, indices, vertex_start, position_accessor.count, index_start, index_accessor.count, has_skin_data ? &skin_vertices : nullptr);

      submeshes.push_back(cooked_submesh{
        static_cast<std::uint32_t>(index_start),
        static_cast<std::uint32_t>(index_accessor.count),
        submesh_volume,
        material_uuid_for(primitive.materialIndex),
        std::move(lods)
      });
    }
  };

  if (!gltf.scenes.empty()) {
    const auto scene_index = gltf.defaultScene.value_or(std::size_t{0});

    fastgltf::iterateSceneNodes(gltf, scene_index, fastgltf::math::fmat4x4{}, [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& world) {
      if (!node.meshIndex.has_value()) {
        return;
      }

      const auto is_skinned = has_skin_data && node.skinIndex.has_value() && node.skinIndex.value() == *primary_skin_index;

      // Skinned vertices stay in bind-pose space -- see the comment above joints' construction --
      // so a skinned node's world transform is never baked in, unlike every other node's.
      append(gltf.meshes[node.meshIndex.value()], is_skinned ? fastgltf::math::fmat4x4{} : world, is_skinned);
    });
  } else {
    for (const auto& gltf_mesh : gltf.meshes) {
      append(gltf_mesh, fastgltf::math::fmat4x4{}, false);
    }
  }

  if (vertices.empty() || indices.empty()) {
    utility::logger<"assets">::warn("Cook: mesh '{}' has no drawable geometry", source.generic_string());
    return false;
  }

  auto animation_clip_original_indices = std::vector<std::uint32_t>{};

  if (has_skin_data) {
    if (!_cook_skeleton(derive_skeleton_uuid(id), joints)) {
      return false;
    }

    for (auto gltf_animation_index = std::size_t{0u}; gltf_animation_index < gltf.animations.size(); ++gltf_animation_index) {
      // included_animations (when non-empty) selects by *original* gltf.animations position, not
      // a renumbered "Nth cooked" count -- derive_animation_clip_uuid below depends on that same
      // original index staying stable across two different future selections of this file (see its
      // doc comment).
      if (!options.included_animations.empty() && std::ranges::find(options.included_animations, gltf_animation_index) == options.included_animations.end()) {
        continue;
      }

      const auto& gltf_animation = gltf.animations[gltf_animation_index];

      auto channels = std::vector<animation_joint_channel>{};

      const auto find_or_create_channel = [&](std::uint32_t joint_index) -> animation_joint_channel& {
        for (auto& existing : channels) {
          if (existing.joint_index == joint_index) {
            return existing;
          }
        }

        auto& created = channels.emplace_back();
        created.joint_index = joint_index;
        return created;
      };

      for (const auto& gltf_channel : gltf_animation.channels) {
        if (!gltf_channel.nodeIndex.has_value()) {
          continue;
        }

        const auto joint_entry = node_to_joint.find(gltf_channel.nodeIndex.value());

        if (joint_entry == node_to_joint.end()) {
          continue; // targets a node that isn't one of this skin's joints -- not skinning-relevant
        }

        const auto& sampler = gltf_animation.samplers[gltf_channel.samplerIndex];

        const auto interpolation = (sampler.interpolation == fastgltf::AnimationInterpolation::Step) ? animation_interpolation::step
          : (sampler.interpolation == fastgltf::AnimationInterpolation::CubicSpline) ? animation_interpolation::cubic_spline
          : animation_interpolation::linear;

        if (interpolation == animation_interpolation::cubic_spline) {
          utility::logger<"assets">::warn("Cook: mesh '{}' animation '{}' uses CUBICSPLINE interpolation, unsupported -- skipping channel", source.generic_string(), std::string{gltf_animation.name.begin(), gltf_animation.name.end()});
          continue;
        }

        auto& channel = find_or_create_channel(static_cast<std::uint32_t>(joint_entry->second));

        auto times = std::vector<std::float_t>{};
        fastgltf::iterateAccessor<std::float_t>(gltf, gltf.accessors[sampler.inputAccessor], [&](std::float_t value) {
          times.push_back(value);
        });

        switch (gltf_channel.path) {
          case fastgltf::AnimationPath::Translation: {
            channel.translation_interpolation = interpolation;
            auto index = std::size_t{0u};
            fastgltf::iterateAccessor<fastgltf::math::fvec3>(gltf, gltf.accessors[sampler.outputAccessor], [&](fastgltf::math::fvec3 value) {
              if (index < times.size()) {
                channel.translation_keys.push_back({times[index], math::vector3{value[0], value[1], value[2]}});
              }
              ++index;
            });
            break;
          }
          case fastgltf::AnimationPath::Rotation: {
            channel.rotation_interpolation = interpolation;
            auto index = std::size_t{0u};
            fastgltf::iterateAccessor<fastgltf::math::fvec4>(gltf, gltf.accessors[sampler.outputAccessor], [&](fastgltf::math::fvec4 value) {
              if (index < times.size()) {
                channel.rotation_keys.push_back({times[index], math::quaternion::wxyz(value[3], value[0], value[1], value[2])});
              }
              ++index;
            });
            break;
          }
          case fastgltf::AnimationPath::Scale: {
            channel.scale_interpolation = interpolation;
            auto index = std::size_t{0u};
            fastgltf::iterateAccessor<fastgltf::math::fvec3>(gltf, gltf.accessors[sampler.outputAccessor], [&](fastgltf::math::fvec3 value) {
              if (index < times.size()) {
                channel.scale_keys.push_back({times[index], math::vector3{value[0], value[1], value[2]}});
              }
              ++index;
            });
            break;
          }
          default:
            break; // Weights (morph targets) -- not applicable to skeletal skinning
        }
      }

      if (channels.empty()) {
        continue; // e.g. an animation that only targets morph-target weights
      }

      auto duration = 0.0f;

      for (const auto& channel : channels) {
        if (!channel.translation_keys.empty()) duration = std::max(duration, channel.translation_keys.back().time);
        if (!channel.rotation_keys.empty()) duration = std::max(duration, channel.rotation_keys.back().time);
        if (!channel.scale_keys.empty()) duration = std::max(duration, channel.scale_keys.back().time);
      }

      auto clip_data = animation_clip_data{};
      clip_data.name = gltf_animation.name.empty() ? fmt::format("clip_{}", gltf_animation_index) : std::string{gltf_animation.name.begin(), gltf_animation.name.end()};
      clip_data.duration = duration;
      clip_data.channels = std::move(channels);

      if (!_cook_animation_clip(derive_animation_clip_uuid(id, static_cast<std::uint32_t>(gltf_animation_index)), clip_data)) {
        return false;
      }

      animation_clip_original_indices.push_back(static_cast<std::uint32_t>(gltf_animation_index));
    }
  }

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write '{}'", cooked.generic_string());
    return false;
  }

  // meshopt-compress both buffers for the on-disk cache (smaller files, less I/O); decoded back to
  // flat vertex/index vectors on read, transparent to everything downstream of _load_cooked_mesh.
  auto encoded_vertices = std::vector<unsigned char>(meshopt_encodeVertexBufferBound(vertices.size(), sizeof(vertex)));
  const auto vertex_data_size = meshopt_encodeVertexBuffer(encoded_vertices.data(), encoded_vertices.size(), vertices.data(), vertices.size(), sizeof(vertex));
  encoded_vertices.resize(vertex_data_size);

  auto encoded_indices = std::vector<unsigned char>(meshopt_encodeIndexBufferBound(indices.size(), vertices.size()));
  const auto index_data_size = meshopt_encodeIndexBuffer(encoded_indices.data(), encoded_indices.size(), indices.data(), indices.size());
  encoded_indices.resize(index_data_size);

  auto header = mesh_file_header{};
  header.magic = mesh_magic;
  header.version = mesh_cook_version;
  header.vertex_count = static_cast<std::uint32_t>(vertices.size());
  header.index_count = static_cast<std::uint32_t>(indices.size());
  header.submesh_count = static_cast<std::uint32_t>(submeshes.size());
  header.bounds_min[0] = mesh_volume.min().x();
  header.bounds_min[1] = mesh_volume.min().y();
  header.bounds_min[2] = mesh_volume.min().z();
  header.bounds_max[0] = mesh_volume.max().x();
  header.bounds_max[1] = mesh_volume.max().y();
  header.bounds_max[2] = mesh_volume.max().z();
  header.vertex_data_size = static_cast<std::uint32_t>(vertex_data_size);
  header.index_data_size = static_cast<std::uint32_t>(index_data_size);
  header.flags = has_skin_data ? mesh_flag_has_skin_data : 0u;
  header.skin_vertex_data_size = static_cast<std::uint32_t>(skin_vertices.size() * sizeof(skin_vertex));
  header.animation_clip_count = static_cast<std::uint32_t>(animation_clip_original_indices.size());

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(reinterpret_cast<const char*>(encoded_vertices.data()), static_cast<std::streamsize>(vertex_data_size));
  out.write(reinterpret_cast<const char*>(encoded_indices.data()), static_cast<std::streamsize>(index_data_size));

  // Raw (unencoded) -- meshopt's vertex codec targets quantizable floats, not packed joint indices.
  if (has_skin_data) {
    out.write(reinterpret_cast<const char*>(skin_vertices.data()), static_cast<std::streamsize>(header.skin_vertex_data_size));
  }

  for (const auto& submesh : submeshes) {
    auto record = submesh_file_record{};
    record.index_offset = submesh.index_offset;
    record.index_count = submesh.index_count;
    record.bounds_min[0] = submesh.bounds.min().x();
    record.bounds_min[1] = submesh.bounds.min().y();
    record.bounds_min[2] = submesh.bounds.min().z();
    record.bounds_max[0] = submesh.bounds.max().x();
    record.bounds_max[1] = submesh.bounds.max().y();
    record.bounds_max[2] = submesh.bounds.max().z();
    record.material_uuid = submesh.material.value();
    record.lod_count = static_cast<std::uint32_t>(submesh.lods.size());

    out.write(reinterpret_cast<const char*>(&record), sizeof(record));

    for (const auto& lod : submesh.lods) {
      auto lod_record = submesh_lod_record{lod.index_offset, lod.index_count, lod.error};
      out.write(reinterpret_cast<const char*>(&lod_record), sizeof(lod_record));
    }
  }

  // One uint32 per cooked clip, its *original* gltf.animations index -- see this file's own doc
  // comment on _load_cooked_mesh for why a bare count isn't enough once clips can be selectively
  // cooked (derive_animation_clip_uuid needs the original index back on every subsequent load, not
  // a renumbered 0..count).
  if (!animation_clip_original_indices.empty()) {
    out.write(reinterpret_cast<const char*>(animation_clip_original_indices.data()), static_cast<std::streamsize>(animation_clip_original_indices.size() * sizeof(std::uint32_t)));
  }

  if (has_skin_data) {
    utility::logger<"assets">::debug("Cooked mesh '{}' -> '{}' ({} joints, {} animation clips)", source.generic_string(), cooked.generic_string(), joints.size(), animation_clip_original_indices.size());
  } else {
    utility::logger<"assets">::debug("Cooked mesh '{}' -> '{}'", source.generic_string(), cooked.generic_string());
  }

  return true;
}

auto asset_cooker::_load_cooked_mesh(const std::filesystem::path& cooked, std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<cooked_submesh>& submeshes, math::volume& bounds, std::vector<skin_vertex>& skin_vertices, std::vector<std::uint32_t>& animation_clip_original_indices) -> bool {
  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = mesh_file_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != mesh_magic || header.version != mesh_cook_version) {
    return false; // missing / corrupt / stale format -> caller recooks
  }

  auto encoded_vertices = std::vector<unsigned char>(header.vertex_data_size);
  in.read(reinterpret_cast<char*>(encoded_vertices.data()), static_cast<std::streamsize>(header.vertex_data_size));

  vertices.resize(header.vertex_count);

  if (!in || meshopt_decodeVertexBuffer(vertices.data(), header.vertex_count, sizeof(vertex), encoded_vertices.data(), encoded_vertices.size()) != 0) {
    return false; // corrupt / truncated -> caller recooks
  }

  auto encoded_indices = std::vector<unsigned char>(header.index_data_size);
  in.read(reinterpret_cast<char*>(encoded_indices.data()), static_cast<std::streamsize>(header.index_data_size));

  indices.resize(header.index_count);

  if (!in || meshopt_decodeIndexBuffer(indices.data(), header.index_count, sizeof(std::uint32_t), encoded_indices.data(), encoded_indices.size()) != 0) {
    return false;
  }

  // Raw (unencoded), immediately after the index data -- matches _cook_mesh's write order exactly;
  // must be read before the submesh records below, not after.
  skin_vertices.clear();

  if ((header.flags & mesh_flag_has_skin_data) != 0u) {
    skin_vertices.resize(header.vertex_count);
    in.read(reinterpret_cast<char*>(skin_vertices.data()), static_cast<std::streamsize>(header.skin_vertex_data_size));

    if (!in) {
      return false;
    }
  }

  submeshes.clear();
  submeshes.reserve(header.submesh_count);

  for (auto i = std::uint32_t{0u}; i < header.submesh_count; ++i) {
    auto record = submesh_file_record{};
    in.read(reinterpret_cast<char*>(&record), sizeof(record));

    if (!in) {
      return false;
    }

    auto lods = std::vector<mesh_lod>{};
    lods.reserve(record.lod_count);

    for (auto l = std::uint32_t{0u}; l < record.lod_count; ++l) {
      auto lod_record = submesh_lod_record{};
      in.read(reinterpret_cast<char*>(&lod_record), sizeof(lod_record));

      if (!in) {
        return false;
      }

      lods.push_back(mesh_lod{lod_record.index_offset, lod_record.index_count, lod_record.error});
    }

    submeshes.push_back(cooked_submesh{
      record.index_offset,
      record.index_count,
      math::volume{math::vector3{record.bounds_min[0], record.bounds_min[1], record.bounds_min[2]}, math::vector3{record.bounds_max[0], record.bounds_max[1], record.bounds_max[2]}},
      math::uuid::from_value(record.material_uuid),
      std::move(lods)
    });
  }

  if (!in) {
    return false;
  }

  bounds = math::volume{math::vector3{header.bounds_min[0], header.bounds_min[1], header.bounds_min[2]}, math::vector3{header.bounds_max[0], header.bounds_max[1], header.bounds_max[2]}};

  // One uint32 per cooked clip, its original gltf.animations index -- see _cook_mesh's write side.
  animation_clip_original_indices.clear();
  animation_clip_original_indices.resize(header.animation_clip_count);

  if (header.animation_clip_count > 0u) {
    in.read(reinterpret_cast<char*>(animation_clip_original_indices.data()), static_cast<std::streamsize>(header.animation_clip_count) * static_cast<std::streamsize>(sizeof(std::uint32_t)));

    if (!in) {
      return false;
    }
  }

  return true;
}

auto asset_cooker::write_cooked_mesh(const std::filesystem::path& cooked, const std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, const std::vector<cooked_submesh>& submeshes, const math::volume& bounds) -> bool {
  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write '{}'", cooked.generic_string());
    return false;
  }

  auto encoded_vertices = std::vector<unsigned char>(meshopt_encodeVertexBufferBound(vertices.size(), sizeof(vertex)));
  const auto vertex_data_size = meshopt_encodeVertexBuffer(encoded_vertices.data(), encoded_vertices.size(), vertices.data(), vertices.size(), sizeof(vertex));
  encoded_vertices.resize(vertex_data_size);

  auto encoded_indices = std::vector<unsigned char>(meshopt_encodeIndexBufferBound(indices.size(), vertices.size()));
  const auto index_data_size = meshopt_encodeIndexBuffer(encoded_indices.data(), encoded_indices.size(), indices.data(), indices.size());
  encoded_indices.resize(index_data_size);

  auto header = mesh_file_header{};
  header.magic = mesh_magic;
  header.version = mesh_cook_version;
  header.vertex_count = static_cast<std::uint32_t>(vertices.size());
  header.index_count = static_cast<std::uint32_t>(indices.size());
  header.submesh_count = static_cast<std::uint32_t>(submeshes.size());
  header.bounds_min[0] = bounds.min().x();
  header.bounds_min[1] = bounds.min().y();
  header.bounds_min[2] = bounds.min().z();
  header.bounds_max[0] = bounds.max().x();
  header.bounds_max[1] = bounds.max().y();
  header.bounds_max[2] = bounds.max().z();
  header.vertex_data_size = static_cast<std::uint32_t>(vertex_data_size);
  header.index_data_size = static_cast<std::uint32_t>(index_data_size);
  header.flags = 0u;
  header.skin_vertex_data_size = 0u;
  header.animation_clip_count = 0u;

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(reinterpret_cast<const char*>(encoded_vertices.data()), static_cast<std::streamsize>(vertex_data_size));
  out.write(reinterpret_cast<const char*>(encoded_indices.data()), static_cast<std::streamsize>(index_data_size));

  for (const auto& submesh : submeshes) {
    auto record = submesh_file_record{};
    record.index_offset = submesh.index_offset;
    record.index_count = submesh.index_count;
    record.bounds_min[0] = submesh.bounds.min().x();
    record.bounds_min[1] = submesh.bounds.min().y();
    record.bounds_min[2] = submesh.bounds.min().z();
    record.bounds_max[0] = submesh.bounds.max().x();
    record.bounds_max[1] = submesh.bounds.max().y();
    record.bounds_max[2] = submesh.bounds.max().z();
    record.material_uuid = submesh.material.value();
    record.lod_count = static_cast<std::uint32_t>(submesh.lods.size());

    out.write(reinterpret_cast<const char*>(&record), sizeof(record));

    for (const auto& lod : submesh.lods) {
      auto lod_record = submesh_lod_record{lod.index_offset, lod.index_count, lod.error};
      out.write(reinterpret_cast<const char*>(&lod_record), sizeof(lod_record));
    }
  }

  utility::logger<"assets">::debug("Wrote generated mesh '{}'", cooked.generic_string());

  return true;
}


} // namespace sbx::assets
