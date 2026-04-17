// SPDX-License-Identifier: MIT
#include <libsbx/physics/mesh_collider.hpp>

#include <algorithm>
#include <numeric>

#include <libsbx/units/bytes.hpp>

#include <libsbx/utility/timer.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::physics {

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

static auto _load_mesh(const aiMesh* mesh, std::vector<math::vector3>& vertices, std::vector<std::uint32_t> indices, const math::matrix4x4& local_transform) -> void {
  vertices.reserve(vertices.size() + mesh->mNumVertices);
  indices.reserve(indices.size() + mesh->mNumFaces * 3u);

  for (auto i = 0u; i < mesh->mNumVertices; ++i) {
    vertices.push_back(math::vector3{local_transform * _convert_vec4(mesh->mVertices[i], 1.0f)});
  }

  for (auto i = 0u; i < mesh->mNumFaces; ++i) {
    indices.push_back(mesh->mFaces[i].mIndices[0]);
    indices.push_back(mesh->mFaces[i].mIndices[1]);
    indices.push_back(mesh->mFaces[i].mIndices[2]);
  }
}

static auto _load_node(const aiNode* node, const aiScene* scene, std::vector<math::vector3>& vertices, std::vector<std::uint32_t> indices, const math::matrix4x4& parent_transform) -> void {
  const auto local_transform = parent_transform * _convert_mat4(node->mTransformation);

  for (auto i = 0u; i < node->mNumMeshes; ++i) {
    _load_mesh(scene->mMeshes[node->mMeshes[i]], vertices, indices, local_transform);
  }

  for (auto i = 0u; i < node->mNumChildren; ++i) {
    _load_node(node->mChildren[i], scene, vertices, indices, local_transform);
  }
}

mesh_collider::mesh_collider(const std::filesystem::path& path) {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  const auto resolved_path = assets_module.resolve_path(path);

  if (!std::filesystem::exists(resolved_path)) {
    throw utility::runtime_error{"Mesh file not found: {}", resolved_path.string()};
  }

  auto timer = utility::timer{};

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

  _load_node(scene->mRootNode, scene, _vertices, _indices, math::matrix4x4::identity);

  _build_bvh();

  const auto vertices_count = _vertices.size();
  const auto indices_count = _indices.size();

  const auto b = units::byte{vertices_count * sizeof(math::vector3)};

  const auto kb = units::quantity_cast<units::kilobyte>(b);

  utility::logger<"models">::debug("Loaded mesh collider: {}, vertices: {}, indices: {}, size: {} kb in {:.2f}ms", resolved_path.string(), vertices_count, indices_count, kb.value(), units::quantity_cast<units::millisecond>(timer.elapsed()));
}

mesh_collider::mesh_collider(std::vector<math::vector3> vertices, std::vector<std::uint32_t> indices)
: _vertices{std::move(vertices)}, 
  _indices{std::move(indices)} {
  utility::assert_that(_indices.size() % 3u == 0u, "Mesh for collider is not triangulated");

  _build_bvh();
}

auto mesh_collider::triangle_count() const noexcept -> std::uint32_t {
  return static_cast<std::uint32_t>(_indices.size() / 3u);
}

auto mesh_collider::get_triangle(std::uint32_t triangle_index) const -> triangle {
  const auto i = triangle_index * 3u;

  return triangle{
    _vertices[_indices[i]],
    _vertices[_indices[i + 1u]],
    _vertices[_indices[i + 2u]]
  };
}

auto mesh_collider::query_bvh(const math::volume& volume) const -> std::vector<std::uint32_t> {
  if (_nodes.empty()) {
    return {};
  }

  auto result = std::vector<std::uint32_t>{};
  result.reserve(32u);

  auto stack = std::array<std::uint32_t, 64u>{};
  auto stack_size = std::size_t{1};

  stack[0] = 0u;

  while (stack_size > 0u) {
    const auto& node = _nodes[stack[--stack_size]];

    if (!math::volume::are_overlapping(volume, node.volume)) {
      continue;
    }

    if (node.count > 0u) {
      for (auto i = 0u; i < node.count; ++i) {
        result.push_back(_triangle_indices[node.first + i]);
      }
    } else {
      stack[stack_size++] = node.first;
      stack[stack_size++] = node.first + 1u;
    }
  }

  return result;
}

auto mesh_collider::_triangle_centroid(std::uint32_t triangle_index) const -> math::vector3 {
  const auto tri = get_triangle(triangle_index);

  return (tri.v0 + tri.v1 + tri.v2) * (1.0f / 3.0f);
}

auto mesh_collider::_triangle_volume(std::uint32_t triangle_index) const -> math::volume {
  const auto tri = get_triangle(triangle_index);

  return {
    math::vector3{std::min({tri.v0.x(), tri.v1.x(), tri.v2.x()}), std::min({tri.v0.y(), tri.v1.y(), tri.v2.y()}), std::min({tri.v0.z(), tri.v1.z(), tri.v2.z()})},
    math::vector3{std::max({tri.v0.x(), tri.v1.x(), tri.v2.x()}), std::max({tri.v0.y(), tri.v1.y(), tri.v2.y()}), std::max({tri.v0.z(), tri.v1.z(), tri.v2.z()})}
  };
}

static constexpr auto bvh_leaf_threshold = std::uint32_t{4};

auto mesh_collider::_build_bvh() -> void {
  const auto number_triangles = triangle_count();

  if (number_triangles == 0u) {
    return;
  }

  _triangle_indices.resize(number_triangles);

  std::iota(_triangle_indices.begin(), _triangle_indices.end(), 0u);

  _nodes.reserve(number_triangles * 2u);
  _nodes.push_back({});

  _build_bvh_recursive(0u, 0u, number_triangles);
}

auto mesh_collider::_build_bvh_recursive(std::uint32_t node_index, std::uint32_t triangle_begin, std::uint32_t triangle_count) -> void {
  auto node_volume = _triangle_volume(_triangle_indices[triangle_begin]);

  for (auto i = 1u; i < triangle_count; ++i) {
    const auto triangle_volume = _triangle_volume(_triangle_indices[triangle_begin + i]);
    const auto combined_volume = math::volume::merge(node_volume, triangle_volume);

    node_volume = combined_volume;
  }

  _nodes[node_index].volume = node_volume;

  if (triangle_count <= bvh_leaf_threshold) {
    _nodes[node_index].first = triangle_begin;
    _nodes[node_index].count = triangle_count;
  
    return;
  }

  const auto extent = node_volume.extend();

  auto split_axis = 0u;

  if (extent.y() > extent.x() && extent.y() > extent.z()) {
    split_axis = 1u;
  } else if (extent.z() > extent.x()) {
    split_axis = 2u;
  }

  auto centroid_sum = 0.0f;

  for (auto i = 0u; i < triangle_count; ++i) {
    centroid_sum += _triangle_centroid(_triangle_indices[triangle_begin + i])[split_axis];
  }

  const auto split_value = centroid_sum / static_cast<std::float_t>(triangle_count);

  auto begin = _triangle_indices.begin() + triangle_begin;
  auto end = _triangle_indices.begin() + triangle_begin + triangle_count;

  auto mid = std::partition(begin, end, [&](std::uint32_t triangle_index) {
    return _triangle_centroid(triangle_index)[split_axis] < split_value;
  });

  auto left_count = static_cast<std::uint32_t>(std::distance(_triangle_indices.begin() + triangle_begin, mid));

  if (left_count == 0u || left_count == triangle_count) {
    left_count = triangle_count / 2u;
  }

  const auto right_count = triangle_count - left_count;

  const auto left_index = static_cast<std::uint32_t>(_nodes.size());

  _nodes.push_back({});
  _nodes.push_back({});

  _nodes[node_index].first = left_index;
  _nodes[node_index].count = 0u;

  _build_bvh_recursive(left_index, triangle_begin, left_count);
  _build_bvh_recursive(left_index + 1u, triangle_begin + left_count, right_count);
}

} // namespace sbx::physics