// SPDX-License-Identifier: MIT
#include <libsbx/models/material.hpp>

#include <libsbx/utility/exception.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>

#include <nlohmann/json.hpp>

namespace sbx::models {

auto texture_slot_hash::operator()(const texture_slot& texture_slot) const noexcept -> std::size_t {
  auto hash = std::size_t{0};

  utility::hash_combine(hash, texture_slot.address_mode_u, texture_slot.address_mode_v, texture_slot.min_filter, texture_slot.mag_filter, texture_slot.anisotropy);
  
  return hash;
}

auto format_for_texture_type(const aiTextureType type) -> graphics::format {
  switch (type) {
    case aiTextureType_BASE_COLOR:
    case aiTextureType_EMISSIVE:
      return graphics::format::r8g8b8a8_srgb;
    case aiTextureType_NORMALS:
    case aiTextureType_DIFFUSE_ROUGHNESS:
    case aiTextureType_HEIGHT:
    default:
      return graphics::format::r8g8b8a8_unorm;
  }
}

auto resolve_texture(scenes::scene& scene, const aiMaterial* ai_material, const aiTextureType type, const std::filesystem::path& root_directory) -> graphics::image2d_handle {
  if (ai_material->GetTextureCount(type) == 0u) {
    return graphics::image2d_handle{};
  }

  auto texture_path = aiString{};
  ai_material->GetTexture(type, 0u, &texture_path);

  const auto raw = std::filesystem::path{texture_path.C_Str()};
  const auto image_path = (raw.is_absolute() ? raw : (root_directory / raw)).lexically_normal();
  const auto key = utility::hashed_string{image_path.string()};

  auto& assets = scene.assets();

  if (!assets.has_image(key)) {
    assets.add_image(key, image_path, format_for_texture_type(type));
  }

  return assets.get_image(key);
}

auto resolve_material(scenes::scene& scene, const aiScene* ai_scene, const std::uint32_t mat_index, const std::filesystem::path& resolved_path, const std::filesystem::path& root_directory, std::unordered_map<std::uint32_t, math::uuid>& material_cache) -> math::uuid {
  if (auto entry = material_cache.find(mat_index); entry != material_cache.end()) {
    return entry->second;
  }

  const auto* ai_material = ai_scene->mMaterials[mat_index];
  const auto key = utility::hashed_string{fmt::format("{}#material_{}", resolved_path.string(), mat_index)};

  auto& assets = scene.assets();

  if (!assets.has_material(key)) {
    auto material = models::material{};

    // Base color
    auto base_color = aiColor4D{1.0f, 1.0f, 1.0f, 1.0f};

    if (ai_material->Get(AI_MATKEY_BASE_COLOR, base_color) == AI_SUCCESS) {
      material.base_color = math::color{base_color.r, base_color.g, base_color.b, base_color.a};
    }

    // Metallic / roughness
    ai_material->Get(AI_MATKEY_METALLIC_FACTOR, material.metallic_factor);
    ai_material->Get(AI_MATKEY_ROUGHNESS_FACTOR, material.roughness_factor);

    // Emissive
    auto emissive = aiColor3D{};

    if (ai_material->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
      material.emissive_factor = math::vector4{emissive.r, emissive.g, emissive.b, 1.0f};
    }

    // Alpha
    auto alpha_mode = aiString{};

    if (ai_material->Get(AI_MATKEY_GLTF_ALPHAMODE, alpha_mode) == AI_SUCCESS) {
      const auto mode = std::string_view{alpha_mode.C_Str()};

      if (mode == "MASK")  { 
        material.alpha = models::alpha_mode::mask;  
      } else if (mode == "BLEND") { 
        material.alpha = models::alpha_mode::blend; 
      }
    }

    ai_material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, material.alpha_cutoff);

    // Double sided
    auto double_sided = false;

    if (ai_material->Get(AI_MATKEY_TWOSIDED, double_sided) == AI_SUCCESS) {
      material.is_double_sided = double_sided;
    }

    // Textures
    material.albedo.image = resolve_texture(scene, ai_material, aiTextureType_BASE_COLOR, root_directory);
    material.normal.image = resolve_texture(scene, ai_material, aiTextureType_NORMALS, root_directory);
    material.metallic_roughness.image = resolve_texture(scene, ai_material, aiTextureType_DIFFUSE_ROUGHNESS, root_directory);
    material.occlusion.image = resolve_texture(scene, ai_material, aiTextureType_LIGHTMAP, root_directory);
    material.emissive.image = resolve_texture(scene, ai_material, aiTextureType_EMISSIVE, root_directory);
    material.height.image = resolve_texture(scene, ai_material, aiTextureType_HEIGHT, root_directory);

    assets.add_material<models::material>(key, std::move(material));
  }

  const auto id = assets.get_material(key);

  material_cache.emplace(mat_index, id);

  return id;
}

auto walk_node(const aiNode* node, const aiScene* ai_scene, scenes::scene& scene, const std::filesystem::path& resolved_path, const std::filesystem::path& root_directory, std::unordered_map<std::uint32_t, math::uuid>& material_cache, std::vector<scenes::static_mesh::submesh>& result) -> void {
  for (auto i = 0u; i < node->mNumMeshes; ++i) {
    const auto* ai_mesh = ai_scene->mMeshes[node->mMeshes[i]];

    const auto index = static_cast<std::uint32_t>(result.size());
    const auto material_id = resolve_material(scene, ai_scene, ai_mesh->mMaterialIndex, resolved_path, root_directory, material_cache);

    result.push_back({index, material_id});
  }

  for (auto i = 0u; i < node->mNumChildren; ++i) {
    walk_node(node->mChildren[i], ai_scene, scene, resolved_path, root_directory, material_cache, result);
  }
}

auto load_materials(const std::filesystem::path& path) -> std::vector<scenes::static_mesh::submesh> {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();

  const auto resolved_path = assets_module.resolve_path(path);

  if (!std::filesystem::exists(resolved_path)) {
    throw utility::runtime_error{"Path does not exist: '{}'", path.string()};
  }

  static constexpr auto import_flags =
    aiProcess_CalcTangentSpace |
    aiProcess_Triangulate |
    aiProcess_SortByPType |
    aiProcess_GenNormals |
    aiProcess_GenUVCoords |
    aiProcess_OptimizeMeshes |
    aiProcess_JoinIdenticalVertices |
    aiProcess_LimitBoneWeights |
    aiProcess_GlobalScale |
    aiProcess_ValidateDataStructure |
    aiProcess_ImproveCacheLocality;

  auto importer = Assimp::Importer{};
  const auto* ai_scene = importer.ReadFile(resolved_path.string(), import_flags);

  if (!ai_scene || (ai_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !ai_scene->mRootNode) {
    throw utility::runtime_error{"load_materials: failed to open '{}': {}", resolved_path.string(), importer.GetErrorString()};
  }

  auto& scene = scenes_module.scene();
  auto& environment = scene.environment();
  auto& graph = scene.graph();

  const auto root_directory = resolved_path.parent_path();

  auto material_cache = std::unordered_map<std::uint32_t, math::uuid>{};
  auto result = std::vector<scenes::static_mesh::submesh>{};

  walk_node(ai_scene->mRootNode, ai_scene, scene, resolved_path, root_directory, material_cache, result);

  return result;
}

} // namespace sbx::models