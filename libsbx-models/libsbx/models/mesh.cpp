// SPDX-License-Identifier: MIT
#include <libsbx/models/mesh.hpp>

#include <filesystem>
#include <fstream>
#include <cstdio>

#include <fmt/format.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <meshoptimizer.h>

#include <libsbx/units/bytes.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/utility/timer.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::models {

static auto _convert_vec2(const aiVector2D& vector) -> math::vector2 {
  return math::vector2{vector.x, vector.y};
}

static auto _convert_vec3(const aiVector3D& vector) -> math::vector3 {
  return math::vector3{vector.x, vector.y, vector.z};
}

static auto _convert_vec4(const aiVector3D& vector, const std::float_t w) -> math::vector4 {
  return math::vector4{vector.x, vector.y, vector.z, w};
}

static auto _convert_mat4(const aiMatrix4x4& matrix) -> math::matrix4x4 {
  auto result = math::matrix4x4{};

  //the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
  result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
  result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
  result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
  result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;

  return result;
}

static auto _calculate_aabb(const std::vector<vertex3d>& vertices) -> math::volume {
  if (vertices.empty()) {
    utility::logger<"models">::warn("Calculating AABB for empty mesh, returning default volume");
    return math::volume{sbx::math::vector3{0.0f, 0.0f, 0.0f}, sbx::math::vector3{0.0f, 0.0f, 0.0f}};
  }

  auto min = vertices.front().position;
  auto max = vertices.front().position;

  for (const auto& vertex : vertices | std::views::drop(1)) {
    min = math::vector3::min(min, vertex.position);
    max = math::vector3::max(max, vertex.position);
  }

  return math::volume{min, max};
}

static auto _load_mesh(const aiMesh* mesh, mesh::mesh_data& data, const math::matrix4x4& local_transform) -> void {
  if (!mesh->HasNormals()) {
    throw std::runtime_error{fmt::format("Mesh '{}' does not have normals", mesh->mName.C_Str())};
  }

  if (!mesh->HasTextureCoords(0)) {
    throw std::runtime_error{fmt::format("Mesh '{}' does not have uvs", mesh->mName.C_Str())};
  }

  if (!mesh->HasTangentsAndBitangents()) {
    throw std::runtime_error{fmt::format("Mesh '{}' does not have tangents", mesh->mName.C_Str())};
  }

  auto submesh = graphics::submesh{};
  submesh.vertex_offset = static_cast<std::uint32_t>(data.vertices.size());
  submesh.index_offset = static_cast<std::uint32_t>(data.indices.size());
  // submesh.index_count = mesh->mNumFaces * 3u;

  auto vertices = std::vector<models::vertex3d>{};
  vertices.reserve(mesh->mNumVertices);

  auto indices = std::vector<std::uint32_t>{};
  indices.reserve(mesh->mNumFaces * 3u);

  const auto normal_matrix = math::matrix4x4::transposed(math::matrix4x4::inverted(local_transform));

  for (auto i = 0u; i < mesh->mNumVertices; ++i) {
    const auto T = _convert_vec3(mesh->mTangents[i]);
    const auto B = _convert_vec3(mesh->mBitangents[i]);
    const auto N = _convert_vec3(mesh->mNormals[i]);

    const auto handedness = (math::vector3::dot(math::vector3::cross(N, T), B) < 0.0f) ? -1.0f : 1.0f;

    auto vertex = models::vertex3d{};
    vertex.position = local_transform * _convert_vec4(mesh->mVertices[i], 1.0f);
    vertex.normal = math::vector3::normalized(normal_matrix * math::vector4{N, 0.0f});
    vertex.uv = _convert_vec3(mesh->mTextureCoords[0][i]);
    vertex.tangent = math::vector4{math::vector3::normalized(normal_matrix * math::vector4{T, 0.0f}), handedness};

    vertices.push_back(vertex);
  }

  // [NOTE] KAJ 2025-07-08 : We need to keep "local" indices here and update them after meshoptimizer has used them
  for (auto i = 0u; i < mesh->mNumFaces; ++i) {
    indices.push_back(mesh->mFaces[i].mIndices[0]);
    indices.push_back(mesh->mFaces[i].mIndices[1]);
    indices.push_back(mesh->mFaces[i].mIndices[2]);
  }

  // Step 1: Generate remap to deduplicate vertices and index
  auto remap = std::vector<std::uint32_t>{};
  remap.resize(indices.size());

  const auto vertex_count = meshopt_generateVertexRemap(remap.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(models::vertex3d));

  // Step 2: Apply the remap to create a unique vertex buffer and remapped indices
  auto unique_vertices = std::vector<models::vertex3d>{};
  unique_vertices.resize(vertex_count);

  auto remapped_indices = std::vector<std::uint32_t>{};
  remapped_indices.resize(indices.size());

  meshopt_remapVertexBuffer(unique_vertices.data(), vertices.data(), vertices.size(), sizeof(models::vertex3d), remap.data());
  meshopt_remapIndexBuffer(remapped_indices.data(), indices.data(), indices.size(), remap.data());

  // Step 3: Optimize index buffer for GPU vertex cache
  meshopt_optimizeVertexCache(remapped_indices.data(), remapped_indices.data(), remapped_indices.size(), vertex_count);

  // Step 4: Overdraw optimization
  meshopt_optimizeOverdraw(remapped_indices.data(), remapped_indices.data(), remapped_indices.size(), &unique_vertices[0].position.x(), vertex_count, sizeof(models::vertex3d), 1.05f);

  // Step 5: Vertex fetch optimization
  meshopt_optimizeVertexFetch(unique_vertices.data(), remapped_indices.data(), remapped_indices.size(), unique_vertices.data(), vertex_count, sizeof(models::vertex3d));

  // [NOTE] KAJ 2025-07-08 : Apply the "global" index offset here
  // std::transform(remapped_indices.begin(), remapped_indices.end(), remapped_indices.begin(), [vertices_count](const auto index) { return index + vertices_count; });

  utility::append(data.vertices, unique_vertices);
  utility::append(data.indices, remapped_indices);

  auto bounds = math::volume{_convert_vec3(mesh->mAABB.mMin), _convert_vec3(mesh->mAABB.mMax)};

  if (bounds.min() == bounds.max()) {
    const auto aabb = math::volume::construct(unique_vertices, &vertex3d::position);

    bounds = aabb;
  }

  data.bounds.include(bounds);

  submesh.index_count = static_cast<std::uint32_t>(data.indices.size()) - submesh.index_offset;
  submesh.vertex_count = static_cast<std::uint32_t>(data.vertices.size()) - submesh.vertex_offset;
  submesh.bounds = bounds;
  submesh.local_transform = local_transform;
  submesh.name = utility::hashed_string{mesh->mName.C_Str()};
  submesh.material_index = mesh->mMaterialIndex;

  data.submeshes.push_back(submesh);
}

static auto _load_node(const aiNode* node, const aiScene* scene, mesh::mesh_data& data, const math::matrix4x4& parent_transform) -> void {
  const auto local_transform = parent_transform * _convert_mat4(node->mTransformation);

  for (auto i = 0u; i < node->mNumMeshes; ++i) {
    _load_mesh(scene->mMeshes[node->mMeshes[i]], data, local_transform);
  }

  for (auto i = 0u; i < node->mNumChildren; ++i) {
    _load_node(node->mChildren[i], scene, data, local_transform);
  }
}

mesh::mesh(const std::filesystem::path& path)
: base{_load(path)} { }

mesh::~mesh() {

}

auto mesh::_load(const std::filesystem::path& path) -> mesh_data {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  const auto resolved_path = assets_module.resolve_path(path);

  if (!std::filesystem::exists(resolved_path)) {
    throw std::runtime_error{"Mesh file not found: " + resolved_path.string()};
  }

  auto timer = utility::timer{};

  auto data = mesh::mesh_data{};

  static const auto import_flags =
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

  const auto* scene = importer.ReadFile(resolved_path.string(), import_flags);

  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
    throw std::runtime_error{fmt::format("Error loading mesh '{}': {}", resolved_path.string(), importer.GetErrorString())};
  }

  _load_node(scene->mRootNode, scene, data, math::matrix4x4::identity);

  const auto vertices_count = data.vertices.size();
  const auto indices_count = data.indices.size();

  const auto b = units::byte{vertices_count * sizeof(vertex3d)};

  const auto kb = units::quantity_cast<units::kilobyte>(b);

  utility::logger<"models">::debug("Loaded mesh: {}, vertices: {}, indices: {}, size: {} kb in {:.2f}ms", resolved_path.string(), vertices_count, indices_count, kb.value(), units::quantity_cast<units::millisecond>(timer.elapsed()));

  return data;
}

static auto calculate_sphere(const std::vector<vertex3d>& vertices) -> math::sphere {
  if (vertices.empty()) {
    utility::logger<"models">::warn("Calculating sphere for empty mesh, returning default sphere");
    return math::sphere{sbx::math::vector3{0.0f, 0.0f, 0.0f}, 0.0f};
  }

  auto center = sbx::math::vector3{0.0f, 0.0f, 0.0f};

  for (const auto& vertex : vertices) {
    center += vertex.position;
  }

  center /= static_cast<float>(vertices.size());

  auto radius = 0.0f;

  for (const auto& vertex : vertices) {
    const auto distance = math::vector3::distance(center, vertex.position);
    radius = std::max(radius, std::abs(distance));
  }

  return math::sphere{center, radius};
}

auto mesh::_process(const std::filesystem::path& path, const mesh_data& data) -> void {
  const auto output_path = std::filesystem::path{path}.replace_extension(".sbxmsh");

  auto output_file = std::ofstream{output_path, std::ios::binary};

  if (!output_file.is_open()) {
    throw std::runtime_error{fmt::format("Failed to open output file: {}", output_path.string())};
  }

  auto header = file_header{};
  header.magic = 69u;
  header.version = 1u;
  header.index_type_size = sizeof(std::uint32_t);
  header.index_count = static_cast<std::uint32_t>(data.indices.size());
  header.vertex_type_size = sizeof(vertex3d);
  header.vertex_count = static_cast<std::uint32_t>(data.vertices.size());
  header.submesh_count = static_cast<std::uint32_t>(data.submeshes.size());

  output_file.write(reinterpret_cast<const char*>(&header), sizeof(file_header));

  output_file.write(reinterpret_cast<const char*>(data.indices.data()), data.indices.size() * sizeof(std::uint32_t));
  output_file.write(reinterpret_cast<const char*>(data.vertices.data()), data.vertices.size() * sizeof(vertex3d));
  output_file.write(reinterpret_cast<const char*>(data.submeshes.data()), data.submeshes.size() * sizeof(graphics::submesh));

  const auto aabb = _calculate_aabb(data.vertices);
  output_file.write(reinterpret_cast<const char*>(&aabb), sizeof(math::volume));

  const auto sphere = calculate_sphere(data.vertices);
  output_file.write(reinterpret_cast<const char*>(&sphere), sizeof(math::sphere));

  utility::logger<"models">::debug("Processed mesh file '{}' to .sbxmsh format", output_path.string());

  output_file.flush();
  output_file.close();
}

} // namespace sbx::models
