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
#include <libsbx/utility/profiler.hpp>

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
  SBX_PROFILE_SCOPE("mesh::_load_mesh");

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

  // Per-submesh per-channel color staging (size == mesh->mNumVertices when present).
  auto submesh_streams = std::array<std::vector<math::vector4>, vertex_stream_count>{};

  for (auto c = std::uint32_t{0u}; c < vertex_stream_count; ++c) {
    if (c < AI_MAX_NUMBER_OF_COLOR_SETS && mesh->HasVertexColors(c)) {
      submesh_streams[c].reserve(mesh->mNumVertices);
    }
  }

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

    for (auto c = std::uint32_t{0u}; c < vertex_stream_count; ++c) {
      if (!submesh_streams[c].capacity()) {
        continue;
      }

      const auto& color = mesh->mColors[c][i];
      submesh_streams[c].push_back(math::vector4{color.r, color.g, color.b, color.a});
    }
  }

  // [NOTE] KAJ 2025-07-08 : We need to keep "local" indices here and update them after meshoptimizer has used them
  for (auto i = 0u; i < mesh->mNumFaces; ++i) {
    indices.push_back(mesh->mFaces[i].mIndices[0]);
    indices.push_back(mesh->mFaces[i].mIndices[1]);
    indices.push_back(mesh->mFaces[i].mIndices[2]);
  }

  SBX_PROFILE_SCOPE_START(s0, "optimize");

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

  // Apply the same remap to any per-vertex streams so they stay aligned with unique_vertices.
  auto unique_streams = std::array<std::vector<math::vector4>, vertex_stream_count>{};

  for (auto c = std::uint32_t{0u}; c < vertex_stream_count; ++c) {
    if (submesh_streams[c].empty()) {
      continue;
    }

    unique_streams[c].resize(vertex_count);
    meshopt_remapVertexBuffer(unique_streams[c].data(), submesh_streams[c].data(), submesh_streams[c].size(), sizeof(math::vector4), remap.data());
  }

  // Step 3: Optimize index buffer for GPU vertex cache
  meshopt_optimizeVertexCache(remapped_indices.data(), remapped_indices.data(), remapped_indices.size(), vertex_count);

  // Step 4: Overdraw optimization
  meshopt_optimizeOverdraw(remapped_indices.data(), remapped_indices.data(), remapped_indices.size(), &unique_vertices[0].position.x(), vertex_count, sizeof(models::vertex3d), 1.05f);

  // Step 5: Vertex fetch optimization.
  // A fetch remap must be applied to the vertex buffer AND every per-vertex stream identically.
  auto fetch_remap = std::vector<std::uint32_t>{};
  fetch_remap.resize(vertex_count);

  meshopt_optimizeVertexFetchRemap(fetch_remap.data(), remapped_indices.data(), remapped_indices.size(), vertex_count);

  auto fetched_vertices = std::vector<models::vertex3d>{};
  fetched_vertices.resize(vertex_count);
  meshopt_remapVertexBuffer(fetched_vertices.data(), unique_vertices.data(), vertex_count, sizeof(models::vertex3d), fetch_remap.data());
  unique_vertices = std::move(fetched_vertices);

  meshopt_remapIndexBuffer(remapped_indices.data(), remapped_indices.data(), remapped_indices.size(), fetch_remap.data());

  for (auto c = std::uint32_t{0u}; c < vertex_stream_count; ++c) {
    if (unique_streams[c].empty()) {
      continue;
    }

    auto fetched = std::vector<math::vector4>{};
    fetched.resize(vertex_count);
    meshopt_remapVertexBuffer(fetched.data(), unique_streams[c].data(), vertex_count, sizeof(math::vector4), fetch_remap.data());
    unique_streams[c] = std::move(fetched);
  }

  SBX_PROFILE_SCOPE_END(s0);

  // [NOTE] KAJ 2025-07-08 : Apply the "global" index offset here
  // std::transform(remapped_indices.begin(), remapped_indices.end(), remapped_indices.begin(), [vertices_count](const auto index) { return index + vertices_count; });

  const auto previous_vertex_count = data.vertices.size();

  utility::append(data.vertices, unique_vertices);
  utility::append(data.indices, remapped_indices);

  // Integrate per-vertex streams into the global data.streams arrays. Any channel that is present on
  // at least one submesh must be dense across every vertex in the final buffer; channels missing on a
  // given submesh are padded with (1, 1, 1, 1) so downstream shaders don't need to special-case gaps.
  for (auto c = std::uint32_t{0u}; c < vertex_stream_count; ++c) {
    const auto submesh_has_stream = !unique_streams[c].empty();

    if (!submesh_has_stream && data.streams[c].empty()) {
      continue;
    }

    if (data.streams[c].size() < previous_vertex_count) {
      data.streams[c].resize(previous_vertex_count, math::vector4{1.0f, 1.0f, 1.0f, 1.0f});
    }

    if (submesh_has_stream) {
      utility::append(data.streams[c], unique_streams[c]);
    } else {
      const auto pad_size = previous_vertex_count + vertex_count;
      data.streams[c].resize(pad_size, math::vector4{1.0f, 1.0f, 1.0f, 1.0f});
    }
  }

  auto bounds = math::volume::construct(unique_vertices, &vertex3d::position);

  data.bounds.include(bounds);

  submesh.index_count = static_cast<std::uint32_t>(data.indices.size()) - submesh.index_offset;
  submesh.vertex_count = static_cast<std::uint32_t>(data.vertices.size()) - submesh.vertex_offset;
  submesh.bounds = bounds;
  submesh.local_transform = local_transform;
  submesh.name = utility::hashed_string{mesh->mName.C_Str()};
  submesh.material_index = mesh->mMaterialIndex;
  submesh.lod_group = static_cast<std::uint32_t>(data.submeshes.size());
  submesh.lod_level = 0u;

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

mesh::mesh(const std::vector<vertex3d>& vertices, const std::vector<std::uint32_t>& indices, const math::volume& bounds)
: base{vertices, indices, bounds} { }

mesh::mesh(std::vector<vertex3d>&& vertices, std::vector<std::uint32_t>&& indices, const math::volume& bounds)
: base{std::move(vertices), std::move(indices), bounds} { }

mesh::mesh(mesh_data&& data)
: base{mesh_data{std::move(data.vertices), std::move(data.indices), std::move(data.submeshes), data.bounds, {}}} {
  _upload_streams(data.streams);
}

mesh::mesh(const std::filesystem::path& path, std::uint32_t lod_count)
: mesh{_load(path, lod_count)} { }

mesh::~mesh() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  for (auto i = std::uint32_t{0u}; i < static_cast<std::uint32_t>(_stream_buffers.size()); ++i) {
    const auto& descriptor = vertex_stream_descriptors[i];

    if (_available_streams.has(descriptor.value)) {
      graphics_module.remove_resource<graphics::buffer>(_stream_buffers[i]);
    }
  }
}

auto mesh::_upload_streams(std::array<std::vector<math::vector4>, vertex_stream_count>& streams) -> void {
  SBX_PROFILE_SCOPE("mesh::_upload_streams");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  for (auto i = std::uint32_t{0u}; i < vertex_stream_count; ++i) {
    auto& source = streams[i];

    if (source.empty()) {
      continue;
    }

    utility::assert_that(source.size() == _vertex_count, fmt::format("Stream {} size {} does not match mesh vertex count {}", i, source.size(), _vertex_count));

    const auto size_in_bytes = source.size() * sizeof(math::vector4);

    _stream_buffers[i] = graphics_module.add_resource<graphics::buffer>(
      size_in_bytes,
      (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    auto staging_buffer = graphics::staging_buffer{size_in_bytes};
    staging_buffer.write(source.data(), size_in_bytes);

    auto command_buffer = graphics::command_buffer{graphics::queue::type::graphics, true};
    auto& target = graphics_module.get_resource<graphics::buffer>(_stream_buffers[i]);

    auto copy_region = VkBufferCopy{};
    copy_region.size = size_in_bytes;
    copy_region.srcOffset = 0u;
    copy_region.dstOffset = 0u;

    command_buffer.copy_buffer(staging_buffer, target, copy_region);
    command_buffer.submit_idle();

    _available_streams.set(vertex_stream_descriptors[i].value);
  }
}

auto mesh::set_stream(vertex_stream stream, std::span<const math::vector4> data) -> void {
  utility::assert_that(data.size() == _vertex_count, fmt::format("Stream size {} does not match mesh vertex count {}", data.size(), _vertex_count));

  const auto index = vertex_stream_index(stream);
  utility::assert_that(!index, "Invalid vertex_stream");

  auto streams = std::array<std::vector<math::vector4>, vertex_stream_count>{};
  streams[*index].assign(data.begin(), data.end());

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (_available_streams.has(stream)) {
    graphics_module.remove_resource<graphics::buffer>(_stream_buffers[*index]);
    _available_streams.clear(stream);
  }

  _upload_streams(streams);
}

auto mesh::stream_address(vertex_stream stream) const -> graphics::buffer::address_type {
  const auto index = vertex_stream_index(stream);

  if (!index || !_available_streams.has(stream)) {
    return graphics::buffer::null;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  return graphics_module.get_resource<graphics::buffer>(_stream_buffers[*index]).address();
}

auto mesh::available_streams() const noexcept -> const utility::bit_field<vertex_stream>& {
  return _available_streams;
}

auto mesh::has_streams(const utility::bit_field<vertex_stream>& required) const noexcept -> bool {
  return (_available_streams.underlying() & required.underlying()) == required.underlying();
}

auto mesh::_load(const std::filesystem::path& path, std::uint32_t lod_count) -> mesh_data {
  SBX_PROFILE_SCOPE("mesh::_load");

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  const auto resolved_path = assets_module.resolve_path(path);

  const auto binary_path = std::filesystem::path{resolved_path}.replace_extension(binary_file_extention);

  if (std::filesystem::exists(binary_path)) {
    const auto source_time = std::filesystem::last_write_time(resolved_path);
    const auto binary_time = std::filesystem::last_write_time(binary_path);

    if (binary_time > source_time) {
      return _load_binary(binary_path);
    }
  }

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

  if (lod_count > 1u) {
    _generate_lods(data, lod_count);
  }

  for (const auto& submesh : data.submeshes) {
    utility::logger<"models">::debug("  submesh '{}': group={}, lod={}, indices={}, vertices={}", submesh.name.str(), submesh.lod_group, submesh.lod_level, submesh.index_count, submesh.vertex_count);
  }

  const auto vertices_count = data.vertices.size();
  const auto indices_count = data.indices.size();

  const auto b = units::byte{vertices_count * sizeof(vertex3d)};
  const auto kb = units::quantity_cast<units::kilobyte>(b);

  utility::logger<"models">::debug("Loaded mesh: {}, vertices: {}, indices: {}, size: {} kb in {:.2f}ms", resolved_path.string(), vertices_count, indices_count, kb.value(), units::quantity_cast<units::millisecond>(timer.elapsed()));

  _process(resolved_path, data);

  return data;
}

auto mesh::_generate_lods(mesh_data& data, std::uint32_t lod_count) -> void {
  const auto base_submesh_count = static_cast<std::uint32_t>(data.submeshes.size());

  for (auto s = std::uint32_t{0u}; s < base_submesh_count; ++s) {
    data.submeshes[s].lod_group = s;
    data.submeshes[s].lod_level = 0u;
  }

  auto target_ratio = 0.5f;
  auto target_error = 0.02f;

  for (auto lod = std::uint32_t{1u}; lod < lod_count; ++lod) {
    for (auto s = std::uint32_t{0u}; s < base_submesh_count; ++s) {
      const auto& base = data.submeshes[s];
      const auto target_index_count = static_cast<std::size_t>(base.index_count * target_ratio);

      auto simplified_indices = std::vector<std::uint32_t>(base.index_count);
      auto result_count = meshopt_simplify(
        simplified_indices.data(),
        data.indices.data() + base.index_offset,
        base.index_count,
        &data.vertices[base.vertex_offset].position.x(),
        base.vertex_count,
        sizeof(vertex3d),
        target_index_count,
        target_error
      );

      target_ratio *= 0.25f;
      target_error *= 1.5f;

      simplified_indices.resize(result_count);

      // Offset indices to account for vertex_offset already being baked into the buffer
      auto lod_submesh = graphics::submesh{};
      lod_submesh.index_count = static_cast<std::uint32_t>(result_count);
      lod_submesh.index_offset = static_cast<std::uint32_t>(data.indices.size());
      lod_submesh.vertex_count = base.vertex_count;
      lod_submesh.vertex_offset = base.vertex_offset;
      lod_submesh.bounds = base.bounds;
      lod_submesh.local_transform = base.local_transform;
      lod_submesh.name = base.name;
      lod_submesh.material_index = base.material_index;
      lod_submesh.lod_level = lod;
      lod_submesh.lod_group = s;

      data.indices.insert(data.indices.end(), simplified_indices.begin(), simplified_indices.end());
      data.submeshes.push_back(lod_submesh);
    }

    target_ratio *= 0.5f;
  }

  // Reorder so LODs are consecutive per group: group0_lod0, group0_lod1, ..., group1_lod0, ...
  std::ranges::stable_sort(data.submeshes, [](const auto& lhs, const auto& rhs) {
    return std::tie(lhs.lod_group, lhs.lod_level) < std::tie(rhs.lod_group, rhs.lod_level);
  });
}

static auto encode_octahedral(const math::vector3& vector) -> std::array<std::int16_t, 2> {
  const auto inverted_l1_norm = 1.0f / (std::abs(vector.x()) + std::abs(vector.y()) + std::abs(vector.z()));

  auto x = vector.x() * inverted_l1_norm;
  auto y = vector.y() * inverted_l1_norm;

  if (vector.z() < 0.0f) {
    const auto temp = x;

    x = (1.0f - std::abs(y)) * (x >= 0.0f ? 1.0f : -1.0f);
    y = (1.0f - std::abs(temp)) * (y >= 0.0f ? 1.0f : -1.0f);
  }

  return {
    static_cast<std::int16_t>(std::round(x * 32767.0f)),
    static_cast<std::int16_t>(std::round(y * 32767.0f))
  };
}

static auto decode_octahedral(const std::array<std::int16_t, 2>& encoded) -> math::vector3 {
  const auto x = static_cast<std::float_t>(encoded[0]) / 32767.0f;
  const auto y = static_cast<std::float_t>(encoded[1]) / 32767.0f;

  auto normal = math::vector3{x, y, 1.0f - std::abs(x) - std::abs(y)};

  if (normal.z() < 0.0f) {
    const auto temp = normal.x();

    normal.x() = (1.0f - std::abs(normal.y())) * (temp >= 0.0f ? 1.0f : -1.0f);
    normal.y() = (1.0f - std::abs(temp)) * (normal.y() >= 0.0f ? 1.0f : -1.0f);
  }

  return math::vector3::normalized(normal);
}

static auto encode_position(const math::vector3& position, const math::volume& bounds) -> std::array<std::int16_t, 3> {
  const auto range = bounds.max() - bounds.min();

  const auto x = (range.x() > 0.0f) ? ((position.x() - bounds.min().x()) / range.x()) * 2.0f - 1.0f : 0.0f;
  const auto y = (range.y() > 0.0f) ? ((position.y() - bounds.min().y()) / range.y()) * 2.0f - 1.0f : 0.0f;
  const auto z = (range.z() > 0.0f) ? ((position.z() - bounds.min().z()) / range.z()) * 2.0f - 1.0f : 0.0f;

  return {
    static_cast<std::int16_t>(std::round(x * 32767.0f)),
    static_cast<std::int16_t>(std::round(y * 32767.0f)),
    static_cast<std::int16_t>(std::round(z * 32767.0f))
  };
}

static auto decode_position(const std::array<std::int16_t, 3>& encoded, const math::volume& bounds) -> math::vector3 {
  const auto x = static_cast<std::float_t>(encoded[0]) / 32767.0f;
  const auto y = static_cast<std::float_t>(encoded[1]) / 32767.0f;
  const auto z = static_cast<std::float_t>(encoded[2]) / 32767.0f;

  const auto range = bounds.max() - bounds.min();

  return math::vector3{
    (x * 0.5f + 0.5f) * range.x() + bounds.min().x(),
    (y * 0.5f + 0.5f) * range.y() + bounds.min().y(),
    (z * 0.5f + 0.5f) * range.z() + bounds.min().z()
  };
}

static auto encode_uv(const math::vector2& uv) -> std::array<std::int16_t, 2> {
  return {
    static_cast<std::int16_t>(std::round(uv.x() * 32767.0f)),
    static_cast<std::int16_t>(std::round(uv.y() * 32767.0f))
  };
}

static auto decode_uv(const std::array<std::int16_t, 2>& encoded) -> math::vector2 {
  return math::vector2{
    static_cast<std::float_t>(encoded[0]) / 32767.0f,
    static_cast<std::float_t>(encoded[1]) / 32767.0f
  };
}

auto mesh::_load_binary(const std::filesystem::path& path) -> mesh_data {
  SBX_PROFILE_SCOPE("mesh::_load_binary");

  auto timer = utility::timer{};

  auto file = std::ifstream{path, std::ios::binary | std::ios::ate};

  if (!file.is_open()) {
    throw std::runtime_error{fmt::format("Failed to open mesh file: {}", path.string())};
  }

  const auto file_size = static_cast<std::size_t>(file.tellg());
  file.seekg(0);

  if (file_size < sizeof(file_header) + sizeof(file_bounds) + sizeof(std::uint32_t)) {
    throw std::runtime_error{fmt::format("Mesh file too small: {}", path.string())};
  }

  auto buffer = std::vector<std::uint8_t>(file_size);
  file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));
  file.close();

  const auto stored_checksum = *reinterpret_cast<const std::uint32_t*>(buffer.data() + file_size - sizeof(std::uint32_t));
  const auto computed_checksum = utility::crc32(std::span{buffer.data(), file_size - sizeof(std::uint32_t)});

  if (stored_checksum != computed_checksum) {
    throw std::runtime_error{fmt::format("Mesh file checksum mismatch: '{}' (got {:08X}, expected {:08X})", path.string(), computed_checksum, stored_checksum)};
  }

  auto cursor = std::size_t{0};

  const auto read = [&]<typename T>(T& dest) {
    std::memcpy(&dest, buffer.data() + cursor, sizeof(T));
    cursor += sizeof(T);
  };

  auto header = file_header{};
  read(header);

  if (header.magic != file_magic) {
    throw std::runtime_error{fmt::format("Invalid mesh magic in '{}' (got {:08X})", path.string(), header.magic)};
  }

  if (header.version != file_version) {
    throw std::runtime_error{fmt::format("Unsupported mesh version {} in '{}'", header.version, path.string())};
  }

  const auto is_compressed = (header.flags & static_cast<std::uint16_t>(file_flags::compressed)) != 0u;
  const auto is_quantized = (header.flags & static_cast<std::uint16_t>(file_flags::quantized)) != 0u;

  auto fb = file_bounds{};
  read(fb);

  const auto global_aabb = math::volume{
    math::vector3{fb.aabb_min[0], fb.aabb_min[1], fb.aabb_min[2]},
    math::vector3{fb.aabb_max[0], fb.aabb_max[1], fb.aabb_max[2]}
  };

  auto data = mesh_data{};

  data.submeshes.reserve(header.submesh_count);

  for (auto i = 0u; i < header.submesh_count; ++i) {
    auto fs = file_submesh{};
    read(fs);

    auto submesh = graphics::submesh{};
    submesh.name = utility::hashed_string{fs.name};
    submesh.material_index = fs.material_index;
    submesh.vertex_offset = fs.vertex_offset;
    submesh.vertex_count = fs.vertex_count;
    submesh.index_offset = fs.index_offset;
    submesh.index_count = fs.index_count;
    submesh.bounds = math::volume{math::vector3{fs.aabb_min[0], fs.aabb_min[1], fs.aabb_min[2]}, math::vector3{fs.aabb_max[0], fs.aabb_max[1], fs.aabb_max[2]}};

    std::memcpy(submesh.local_transform.data(), fs.local_transform, sizeof(fs.local_transform));

    submesh.lod_level = fs.lod_level;
    submesh.lod_group = fs.lod_group;

    data.submeshes.push_back(submesh);
  }

  const auto indices_size = header.index_count * sizeof(std::uint32_t);
  const auto file_vertices_size = header.vertex_count * (is_quantized ? sizeof(file_vertex) : sizeof(vertex3d));

  data.indices.resize(header.index_count);
  data.vertices.resize(header.vertex_count);

  const auto decode_vertices = [&](const std::uint8_t* vertex_source) {
    if (is_quantized) {
      for (auto i = 0u; i < header.vertex_count; ++i) {
        auto file_vert = file_vertex{};
        std::memcpy(&file_vert, vertex_source + i * sizeof(file_vertex), sizeof(file_vertex));

        auto& vertex = data.vertices[i];
        vertex.position = decode_position({file_vert.position[0], file_vert.position[1], file_vert.position[2]}, global_aabb);
        vertex.normal = decode_octahedral({file_vert.normal[0], file_vert.normal[1]});

        const auto decoded_uv = decode_uv({file_vert.uv[0], file_vert.uv[1]});
        vertex.uv = math::vector3{decoded_uv.x(), decoded_uv.y(), 0.0f};

        const auto decoded_tangent = decode_octahedral({file_vert.tangent[0], file_vert.tangent[1]});
        vertex.tangent = math::vector4{decoded_tangent.x(), decoded_tangent.y(), decoded_tangent.z(), static_cast<std::float_t>(file_vert.tangent_w)};
      }
    } else {
      std::memcpy(data.vertices.data(), vertex_source, file_vertices_size);
    }
  };

  if (is_compressed) {
    const auto payload = utility::decompress<std::uint8_t, utility::compression_type::zstd>(std::span{reinterpret_cast<const char*>(buffer.data() + cursor), header.compressed_size}, header.uncompressed_size);

    std::memcpy(data.indices.data(), payload.data(), indices_size);
    decode_vertices(payload.data() + indices_size);

    cursor += header.compressed_size;
  } else {
    std::memcpy(data.indices.data(), buffer.data() + cursor, indices_size);
    decode_vertices(buffer.data() + cursor + indices_size);

    cursor += indices_size + file_vertices_size;
  }

  if ((header.flags & static_cast<std::uint16_t>(file_flags::has_streams)) != 0u) {
    auto stream_mask = std::uint32_t{0u};
    read(stream_mask);

    for (auto c = std::uint32_t{0u}; c < vertex_stream_count; ++c) {
      if ((stream_mask & static_cast<std::uint32_t>(vertex_stream_descriptors[c].value)) == 0u) {
        continue;
      }

      auto& stream = data.streams[c];
      stream.resize(header.vertex_count);

      const auto bytes = header.vertex_count * sizeof(math::vector4);
      std::memcpy(stream.data(), buffer.data() + cursor, bytes);
      cursor += bytes;
    }
  }

  data.bounds = global_aabb;

  auto stream_count = std::uint32_t{0u};

  for (const auto& stream : data.streams) {
    if (!stream.empty()) {
      ++stream_count;
    }
  }

  utility::logger<"models">::debug("Loaded '{}': {} vertices, {} indices, {} submeshes, {} streams in {:.2f}ms", path.string(), data.vertices.size(), data.indices.size(), data.submeshes.size(), stream_count, units::quantity_cast<units::millisecond>(timer.elapsed()));

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

  center /= static_cast<std::float_t>(vertices.size());

  auto radius = 0.0f;

  for (const auto& vertex : vertices) {
    const auto distance = math::vector3::distance(center, vertex.position);
    radius = std::max(radius, std::abs(distance));
  }

  return math::sphere{center, radius};
}

auto mesh::_process(const std::filesystem::path& path, const mesh_data& data) -> void {
  SBX_PROFILE_SCOPE("mesh::_process");

  const auto output_path = std::filesystem::path{path}.replace_extension(binary_file_extention);

  const auto submeshes_size = data.submeshes.size() * sizeof(file_submesh);
  const auto indices_size = data.indices.size() * sizeof(std::uint32_t);
  const auto vertices_size = data.vertices.size() * sizeof(file_vertex);

  const auto append_to = [](auto& buffer, const auto* source, const std::size_t size) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(source);
    buffer.insert(buffer.end(), bytes, bytes + size);
  };

  const auto aabb = _calculate_aabb(data.vertices);
  const auto sphere = calculate_sphere(data.vertices);

  auto encoded_vertices = std::vector<file_vertex>{};
  encoded_vertices.reserve(data.vertices.size());

  for (const auto& vertex : data.vertices) {
    auto file_vert = file_vertex{};

    const auto encoded_position = encode_position(vertex.position, aabb);
    file_vert.position[0] = encoded_position[0];
    file_vert.position[1] = encoded_position[1];
    file_vert.position[2] = encoded_position[2];

    const auto encoded_normal = encode_octahedral(vertex.normal);
    file_vert.normal[0] = encoded_normal[0];
    file_vert.normal[1] = encoded_normal[1];

    const auto encoded_uv = encode_uv(math::vector2{vertex.uv.x(), vertex.uv.y()});
    file_vert.uv[0] = encoded_uv[0];
    file_vert.uv[1] = encoded_uv[1];

    const auto encoded_tangent = encode_octahedral(math::vector3{vertex.tangent.x(), vertex.tangent.y(), vertex.tangent.z()});
    file_vert.tangent[0] = encoded_tangent[0];
    file_vert.tangent[1] = encoded_tangent[1];

    file_vert.tangent_w = static_cast<std::int8_t>(vertex.tangent.w() >= 0.0f ? 1 : -1);
    file_vert._pad = 0u;

    encoded_vertices.push_back(file_vert);
  }

  auto payload = std::vector<std::uint8_t>{};
  payload.reserve(indices_size + vertices_size);

  append_to(payload, data.indices.data(), indices_size);
  append_to(payload, encoded_vertices.data(), vertices_size);

  const auto compressed = utility::compress<std::uint8_t, utility::compression_type::zstd>(std::span{payload});

  auto header = file_header{};
  header.magic = file_magic;
  header.version = file_version;
  header.flags = static_cast<std::uint16_t>(file_flags::compressed) | static_cast<std::uint16_t>(file_flags::quantized);
  header.vertex_count = static_cast<std::uint32_t>(data.vertices.size());
  header.index_count = static_cast<std::uint32_t>(data.indices.size());
  header.submesh_count = static_cast<std::uint32_t>(data.submeshes.size());
  header.vertex_stride = static_cast<std::uint32_t>(sizeof(file_vertex));
  header.index_stride = static_cast<std::uint32_t>(sizeof(std::uint32_t));
  header.uncompressed_size = static_cast<std::uint32_t>(payload.size());
  header.compressed_size = static_cast<std::uint32_t>(compressed.size());

  auto stream_bytes = std::size_t{0u};

  for (auto c = std::uint32_t{0u}; c < vertex_stream_count; ++c) {
    if (data.streams[c].size() == data.vertices.size() && !data.streams[c].empty()) {
      stream_bytes += data.streams[c].size() * sizeof(math::vector4);
    }
  }

  if (stream_bytes > 0u) {
    stream_bytes += sizeof(std::uint32_t); // stream_mask header
  }

  const auto total_bytes = sizeof(file_header) + sizeof(file_bounds) + submeshes_size + compressed.size() + stream_bytes + sizeof(std::uint32_t);

  auto buffer = std::vector<std::uint8_t>{};
  buffer.reserve(total_bytes);

  append_to(buffer, &header, sizeof(header));

  auto bounds = file_bounds{};
  bounds.aabb_min[0] = aabb.min().x();
  bounds.aabb_min[1] = aabb.min().y();
  bounds.aabb_min[2] = aabb.min().z();
  bounds.aabb_max[0] = aabb.max().x();
  bounds.aabb_max[1] = aabb.max().y();
  bounds.aabb_max[2] = aabb.max().z();
  bounds.sphere_center[0] = sphere.center().x();
  bounds.sphere_center[1] = sphere.center().y();
  bounds.sphere_center[2] = sphere.center().z();
  bounds.sphere_radius = sphere.radius();

  append_to(buffer, &bounds, sizeof(bounds));

  for (const auto& submesh : data.submeshes) {
    auto entry = file_submesh{};

    std::strncpy(entry.name, submesh.name.c_str(), sizeof(entry.name) - 1u);
    entry.name[sizeof(entry.name) - 1u] = '\0';

    entry.material_index = submesh.material_index;
    entry.vertex_offset = submesh.vertex_offset;
    entry.vertex_count = submesh.vertex_count;
    entry.index_offset = submesh.index_offset;
    entry.index_count = submesh.index_count;
    entry.aabb_min[0] = submesh.bounds.min().x();
    entry.aabb_min[1] = submesh.bounds.min().y();
    entry.aabb_min[2] = submesh.bounds.min().z();
    entry.aabb_max[0] = submesh.bounds.max().x();
    entry.aabb_max[1] = submesh.bounds.max().y();
    entry.aabb_max[2] = submesh.bounds.max().z();

    std::memcpy(entry.local_transform, submesh.local_transform.data(), sizeof(entry.local_transform));

    entry.lod_level = submesh.lod_level;
    entry.lod_group = submesh.lod_group;

    append_to(buffer, &entry, sizeof(entry));
  }

  append_to(buffer, compressed.data(), compressed.size());

  // Streams block: uint32_t mask, then for each set bit a dense float4 array sized vertex_count.
  auto stream_mask = std::uint32_t{0u};

  for (auto c = std::uint32_t{0u}; c < vertex_stream_count; ++c) {
    if (data.streams[c].size() == data.vertices.size() && !data.streams[c].empty()) {
      stream_mask |= static_cast<std::uint32_t>(vertex_stream_descriptors[c].value);
    }
  }

  if (stream_mask != 0u) {
    header.flags |= static_cast<std::uint16_t>(file_flags::has_streams);

    append_to(buffer, &stream_mask, sizeof(stream_mask));

    for (auto c = std::uint32_t{0u}; c < vertex_stream_count; ++c) {
      if ((stream_mask & static_cast<std::uint32_t>(vertex_stream_descriptors[c].value)) == 0u) {
        continue;
      }

      append_to(buffer, data.streams[c].data(), data.streams[c].size() * sizeof(math::vector4));
    }

    // Rewrite header flags in the already-appended buffer.
    std::memcpy(buffer.data() + offsetof(file_header, flags), &header.flags, sizeof(header.flags));
  }

  const auto checksum = utility::crc32(buffer);

  append_to(buffer, &checksum, sizeof(checksum));

  auto file = std::ofstream{output_path, std::ios::binary};

  if (!file.is_open()) {
    throw std::runtime_error{fmt::format("Failed to open output file: {}", output_path.string())};
  }

  file.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));

  utility::logger<"models">::debug("Wrote '{}': {} vertices, {} indices, {} submeshes, stream_mask 0x{:02X} — {} bytes (compressed from {} bytes)", output_path.string(), data.vertices.size(), data.indices.size(), data.submeshes.size(), stream_mask, buffer.size(), payload.size());
}

} // namespace sbx::models
