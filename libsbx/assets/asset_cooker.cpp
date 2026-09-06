// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/asset_cooker.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <numeric>
#include <string_view>
#include <system_error>
#include <unordered_map>

#include <yaml-cpp/yaml.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/math.hpp>

#include <meshoptimizer.h>

#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/fourcc.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/hash.hpp>

#include <libsbx/math/matrix_cast.hpp>

#include <libsbx/core/engine.hpp>

namespace sbx::assets {

inline constexpr auto texture_magic = utility::fourcc_v<"SBTX">;  // 'SBTX'
inline constexpr auto texture_version = std::uint32_t{1u};

inline constexpr auto mesh_magic = utility::fourcc_v<"SBSH">;   // 'SBSH'
inline constexpr auto mesh_version = std::uint32_t{8u}; // v8: generates flat normals for primitives missing NORMAL (previously left zeroed)

inline constexpr auto material_magic = utility::fourcc_v<"SBMT">; // 'SBMT'
inline constexpr auto material_version = std::uint32_t{3u};

inline constexpr auto environment_magic = utility::fourcc_v<"SBEN">; // 'SBEN'
inline constexpr auto environment_version = std::uint32_t{1u};

inline constexpr auto skeleton_magic = utility::fourcc_v<"SBSK">; // 'SBSK'
inline constexpr auto skeleton_version = std::uint32_t{1u};

inline constexpr auto animation_magic = utility::fourcc_v<"SBAN">; // 'SBAN'
inline constexpr auto animation_version = std::uint32_t{1u};

// A mesh cook also emits its materials and, for a skinned mesh, its skeleton/animation clips --
// so a mesh blob's freshness depends on all four cookers.
inline constexpr auto mesh_cooker_version = mesh_version * 1000000u + material_version * 10000u + skeleton_version * 100u + animation_version;

struct texture_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t channels;   // always 4 (RGBA) for now
  std::uint32_t data_size;  // bytes of pixel data following the header
}; // struct texture_header

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
  std::uint32_t animation_clip_count;  // clips cooked as a side effect, resolvable via _derive_animation_clip_uuid(id, 0..count)
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

struct material_file_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::float_t base_color_factor[4];
  std::float_t emissive_factor[3];
  std::float_t metallic_factor;
  std::float_t roughness_factor;
  std::uint32_t alpha_mode;
  std::float_t alpha_cutoff;
  std::uint32_t is_double_sided;
  std::float_t normal_scale;
  std::float_t occlusion_strength;
  std::float_t emissive_strength;
  std::float_t ior;
  std::uint64_t albedo_uuid;
  std::uint64_t normal_uuid;
  std::uint64_t metallic_roughness_uuid;
  std::uint64_t occlusion_uuid;
  std::uint64_t emissive_uuid;
  std::uint32_t name_length;
}; // struct material_file_header

struct skeleton_file_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::uint32_t joint_count;
}; // struct skeleton_file_header

// Immediately followed by name_length bytes of the joint's name.
struct skeleton_joint_record {
  std::int32_t parent_index; // -1 = root; always < this joint's own index (topologically sorted)
  std::float_t inverse_bind_matrix[16]; // column-major, matches math::matrix4x4's layout
  std::float_t bind_translation[3];
  std::float_t bind_rotation[4]; // x, y, z, w
  std::float_t bind_scale[3];
  std::uint32_t name_length;
}; // struct skeleton_joint_record

struct animation_clip_file_header {
  std::uint32_t magic;
  std::uint32_t version;
  std::float_t duration;
  std::uint32_t channel_count;
  std::uint32_t name_length; // name bytes immediately follow this header
}; // struct animation_clip_file_header

// Immediately followed by translation_key_count vector3_key_records, then rotation_key_count
// quaternion_key_records, then scale_key_count vector3_key_records.
struct animation_channel_record {
  std::uint32_t joint_index;
  std::uint32_t translation_key_count;
  std::uint32_t rotation_key_count;
  std::uint32_t scale_key_count;
  std::uint32_t translation_interpolation;
  std::uint32_t rotation_interpolation;
  std::uint32_t scale_interpolation;
}; // struct animation_channel_record

struct vector3_key_record {
  std::float_t time;
  std::float_t value[3];
}; // struct vector3_key_record

struct quaternion_key_record {
  std::float_t time;
  std::float_t value[4]; // x, y, z, w
}; // struct quaternion_key_record

asset_cooker::~asset_cooker() {
  _save_manifest();
}

auto asset_cooker::import(const std::filesystem::path& path) -> math::uuid {
  ensure_manifest_loaded();

  const auto key = path.generic_string();

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _uuids.find(key); entry != _uuids.end()) {
      return entry->second;
    }
  }

  const auto uuid = _read_or_create_meta(path);

  {
    auto lock = std::lock_guard{_mutex};

    _uuids.emplace(key, uuid);
    _paths.emplace(uuid, path);

    auto& entry = _manifest[uuid];
    if (entry.path.empty()) {
      entry.path = path;
      _manifest_dirty = true;
    }
  }

  return uuid;
}

auto asset_cooker::import_directory(const std::filesystem::path& root) -> void {
  ensure_manifest_loaded();

  if (!std::filesystem::exists(root)) {
    utility::logger<"assets">::warn("Asset root '{}' does not exist", root.generic_string());

    return;
  }

  for (const auto& entry : std::filesystem::recursive_directory_iterator{root}) {
    if (!entry.is_regular_file()) {
      continue;
    }

    const auto& path = entry.path();

    auto extension = path.extension().string();

    std::ranges::transform(extension, extension.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".gltf" || extension == ".glb" || extension == ".material" || extension == ".hdr" || extension == ".particle_effect" || extension == ".animation_graph") {
      import(entry.path());
    }
  }
}

auto asset_cooker::path_of(const math::uuid& id) const -> std::filesystem::path {
  auto lock = std::lock_guard{_mutex};

  if (const auto entry = _paths.find(id); entry != _paths.end()) {
    return entry->second;
  }

  return {};
}

auto asset_cooker::ensure_manifest_loaded() -> void {
  {
    auto lock = std::lock_guard{_mutex};

    if (_manifest_loaded) {
      return;
    }

    _manifest_loaded = true;
  }

  _load_manifest();
}

auto asset_cooker::absolute(const std::filesystem::path& relative) -> std::filesystem::path {
  const auto& project = core::engine::project();

  return project.assets_directory() / relative;
}

// Inverse of absolute(): converts a resolved path back to one relative to assets_directory(),
// for storing assets-relative paths (e.g. in a .material file's texture slots).
auto asset_cooker::relative(const std::filesystem::path& absolute) -> std::filesystem::path {
  const auto& project = core::engine::project();

  return std::filesystem::relative(absolute, project.assets_directory());
}

auto asset_cooker::resolve_texture(const math::uuid& id) -> std::optional<pixel_data> {
  ensure_manifest_loaded();

  auto path = std::filesystem::path{};
  {
    auto lock = std::lock_guard{_mutex};

    const auto entry = _paths.find(id);

    if (entry == _paths.end()) {
      utility::logger<"assets">::warn("Unknown texture uuid {}", id);
      return std::nullopt;
    }

    path = entry->second;
  }

  const auto cooked = _cooked_path(id, ".sbxtex");

  if (_is_cooked_stale(id, path, cooked, texture_version)) {
    if (!_cook_texture(path, cooked)) {
      utility::logger<"assets">::warn("Could not cook texture '{}'", path.generic_string());
      return std::nullopt;
    }
    _record_cook(id, texture_version, path);
  }

  auto data = pixel_data{};

  // If the blob is unreadable/out-of-date (e.g. cooker version bumped), recook once.
  if (!_load_cooked_texture(cooked, data.pixels, data.width, data.height)) {
    if (!_cook_texture(path, cooked) || !_load_cooked_texture(cooked, data.pixels, data.width, data.height)) {
      utility::logger<"assets">::warn("Could not load cooked texture '{}'", cooked.generic_string());
      return std::nullopt;
    }
    _record_cook(id, texture_version, path);
  }

  return data;
}

auto asset_cooker::resolve_environment(const math::uuid& id) -> std::optional<pixel_data> {
  ensure_manifest_loaded();

  auto path = std::filesystem::path{};
  {
    auto lock = std::lock_guard{_mutex};
    const auto entry = _paths.find(id);
    if (entry == _paths.end()) {
      utility::logger<"assets">::warn("Unknown environment map uuid {}", id);
      return std::nullopt;
    }
    path = entry->second;
  }

  const auto cooked = _cooked_path(id, ".sbxenv");

  if (_is_cooked_stale(id, path, cooked, environment_version)) {
    if (!_cook_environment_map(path, cooked)) {
      utility::logger<"assets">::warn("Could not cook environment map '{}'", path.generic_string());
      return std::nullopt;
    }
    _record_cook(id, environment_version, path);
  }

  auto data = pixel_data{};

  if (!_load_cooked_environment_map(cooked, data.pixels, data.width, data.height)) {
    if (!_cook_environment_map(path, cooked) || !_load_cooked_environment_map(cooked, data.pixels, data.width, data.height)) {
      utility::logger<"assets">::warn("Could not load cooked environment map '{}'", cooked.generic_string());
      return std::nullopt;
    }
    _record_cook(id, environment_version, path);
  }

  return data;
}

auto asset_cooker::resolve_mesh(const math::uuid& id, const mesh_import_options& options, const material_resolver& resolve_material) -> std::optional<cooked_mesh_data> {
  ensure_manifest_loaded();

  auto source = std::filesystem::path{};
  {
    auto lock = std::lock_guard{_mutex};

    const auto entry = _paths.find(id);

    if (entry == _paths.end()) {
      utility::logger<"assets">::warn("Unknown mesh uuid {}", id);
      return std::nullopt;
    }

    source = entry->second;
  }

  const auto cooked = _cooked_path(id, ".sbxmsh");

  if (_is_cooked_stale(id, source, cooked, mesh_cooker_version)) {
    if (!_cook_mesh(source, id, cooked, options, resolve_material)) {
      return std::nullopt;
    }
    _record_cook(id, mesh_cooker_version, source);
  }

  auto data = cooked_mesh_data{};
  auto animation_clip_count = std::uint32_t{0u};

  if (!_load_cooked_mesh(cooked, data.vertices, data.indices, data.submeshes, data.bounds, data.skin_vertices, animation_clip_count)) {
    if (!_cook_mesh(source, id, cooked, options, resolve_material) || !_load_cooked_mesh(cooked, data.vertices, data.indices, data.submeshes, data.bounds, data.skin_vertices, animation_clip_count)) {
      utility::logger<"assets">::warn("Could not load cooked mesh '{}'", cooked.generic_string());
      return std::nullopt;
    }
    _record_cook(id, mesh_cooker_version, source);
  }

  if (data.vertices.empty() || data.indices.empty()) {
    utility::logger<"assets">::warn("Mesh '{}' has no drawable geometry", source.generic_string());
    return std::nullopt;
  }

  if (!data.skin_vertices.empty()) {
    data.skeleton = _derive_skeleton_uuid(id);

    data.animation_clips.reserve(animation_clip_count);

    for (auto index = std::uint32_t{0u}; index < animation_clip_count; ++index) {
      data.animation_clips.push_back(_derive_animation_clip_uuid(id, index));
    }
  }

  return data;
}

auto asset_cooker::resolve_cooked_material(const math::uuid& id) -> std::optional<material_description> {
  const auto cooked = _cooked_path(id, ".sbxmat");

  if (!std::filesystem::exists(cooked)) {
    return std::nullopt;
  }

  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    utility::logger<"assets">::warn("Could not open cooked material '{}'", cooked.generic_string());
    return std::nullopt;
  }

  auto header = material_file_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != material_magic || header.version != material_version) {
    utility::logger<"assets">::warn("Invalid cooked material '{}'", cooked.generic_string());
    return std::nullopt;
  }

  auto name = std::string(header.name_length, '\0');

  if (header.name_length > 0u) {
    in.read(name.data(), static_cast<std::streamsize>(header.name_length));

    if (!in) {
      return std::nullopt;
    }
  }

  auto description = material_description{};
  description.name = name.empty() ? std::string{"material"} : name;
  description.base_color_factor = math::color{header.base_color_factor[0], header.base_color_factor[1], header.base_color_factor[2], header.base_color_factor[3]};
  description.emissive_factor = math::vector3{header.emissive_factor[0], header.emissive_factor[1], header.emissive_factor[2]};
  description.metallic_factor = header.metallic_factor;
  description.roughness_factor = header.roughness_factor;
  description.alpha = static_cast<alpha_mode>(header.alpha_mode);
  description.alpha_cutoff = header.alpha_cutoff;
  description.is_double_sided = header.is_double_sided != 0u;
  description.normal_scale = header.normal_scale;
  description.occlusion_strength = header.occlusion_strength;
  description.emissive_strength = header.emissive_strength;
  description.ior = header.ior;
  description.albedo = math::uuid::from_value(header.albedo_uuid);
  description.normal = math::uuid::from_value(header.normal_uuid);
  description.metallic_roughness = math::uuid::from_value(header.metallic_roughness_uuid);
  description.occlusion = math::uuid::from_value(header.occlusion_uuid);
  description.emissive = math::uuid::from_value(header.emissive_uuid);

  return description;
}

auto asset_cooker::resolve_skeleton(const math::uuid& id) -> std::optional<std::vector<skeleton::joint>> {
  auto joints = std::vector<skeleton::joint>{};

  if (!_load_cooked_skeleton(id, joints)) {
    return std::nullopt;
  }

  return joints;
}

auto asset_cooker::resolve_animation_clip(const math::uuid& id) -> std::optional<animation_clip_data> {
  auto data = animation_clip_data{};

  if (!_load_cooked_animation_clip(id, data)) {
    return std::nullopt;
  }

  return data;
}

auto asset_cooker::_read_or_create_meta(const std::filesystem::path& path) -> math::uuid {
  auto meta_path = path;
  meta_path += ".meta";

  if (std::filesystem::exists(meta_path)) {
    try {
      const auto node = YAML::LoadFile(meta_path.string());
      return node["uuid"].as<math::uuid>();
    } catch (const std::exception& exception) {
      utility::logger<"assets">::warn("Invalid meta '{}' ({}); regenerating", meta_path.generic_string(), exception.what());
    }
  }

  const auto uuid = math::uuid::create();

  auto node = YAML::Node{};
  node["uuid"] = uuid;

  auto out = std::ofstream{meta_path};
  out << node;

  utility::logger<"assets">::debug("Imported '{}' as {}", path.generic_string(), uuid);

  return uuid;
}

auto asset_cooker::_cooked_path(const math::uuid& id, std::string_view extension) const -> std::filesystem::path {
  const auto& project = core::engine::project();

  return project.library_directory() / fmt::format("{}{}", id.value(), extension);
}

auto asset_cooker::_is_cooked_stale(const math::uuid& id, const std::filesystem::path& source, const std::filesystem::path& cooked, std::uint32_t cooker_version) -> bool {
  if (!std::filesystem::exists(cooked)) {
    return true;
  }

  auto lock = std::lock_guard{_mutex};

  const auto entry = _manifest.find(id);

  if (entry == _manifest.end() || entry->second.cooker_version != cooker_version) {
    return true;
  }

  auto error = std::error_code{};
  const auto mtime = std::filesystem::last_write_time(source, error);

  if (error) {
    return true;
  }

  const auto mtime_count = mtime.time_since_epoch().count();

  if (entry->second.source_mtime == mtime_count) {
    return false; // fast path: unchanged since last cook
  }

  // mtime moved — confirm with a content hash before recooking.
  if (entry->second.source_hash == utility::hash_file(source)) {
    entry->second.source_mtime = mtime_count; // touched, not changed
    _manifest_dirty = true;
    return false;
  }

  return true;
}

auto asset_cooker::_record_cook(const math::uuid& id, std::uint32_t cooker_version, const std::filesystem::path& source) -> void {
  auto error = std::error_code{};
  const auto mtime = std::filesystem::last_write_time(source, error);
  const auto mtime_count = error ? std::int64_t{0} : mtime.time_since_epoch().count();
  const auto hash = utility::hash_file(source);

  {
    auto lock = std::lock_guard{_mutex};

    auto& entry = _manifest[id];
    entry.cooker_version = cooker_version;
    entry.source_hash = hash;
    entry.source_mtime = mtime_count;
    _manifest_dirty = true;
  }

  _save_manifest();
}

auto asset_cooker::_manifest_path() const -> std::filesystem::path {
  return core::engine::project().library_directory() / "manifest.yaml";
}

auto asset_cooker::_load_manifest() -> void {
  const auto path = _manifest_path();

  if (!std::filesystem::exists(path)) {
    return;
  }

  auto root = YAML::Node{};

  try {
    root = YAML::LoadFile(path.string());
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Could not read asset manifest '{}' ({})", path.generic_string(), exception.what());
    return;
  }

  const auto assets = root["assets"];

  if (!assets) {
    return;
  }

  auto lock = std::lock_guard{_mutex};

  for (const auto node : assets) {
    const auto uuid = node["uuid"].as<math::uuid>();

    auto entry = manifest_entry{};
    entry.path = node["path"].as<std::string>();
    entry.cooker_version = node["cooker_version"].as<std::uint32_t>();
    entry.source_hash = node["source_hash"].as<std::uint64_t>();
    entry.source_mtime = node["source_mtime"].as<std::int64_t>();

    _uuids.emplace(entry.path.generic_string(), uuid);
    _paths.emplace(uuid, entry.path);
    _manifest.emplace(uuid, std::move(entry));
  }

  utility::logger<"assets">::debug("Loaded asset manifest: {} entries", _manifest.size());
}

auto asset_cooker::_save_manifest() -> void {
  auto lock = std::lock_guard{_mutex};

  if (!_manifest_dirty) {
    return;
  }

  auto emitter = YAML::Emitter{};

  emitter << YAML::BeginMap;
  emitter << YAML::Key << "version" << YAML::Value << 1u;
  emitter << YAML::Key << "assets" << YAML::Value << YAML::BeginSeq;

  for (const auto& [uuid, entry] : _manifest) {
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "uuid" << YAML::Value << uuid.value();
    emitter << YAML::Key << "path" << YAML::Value << entry.path.generic_string();
    emitter << YAML::Key << "cooker_version" << YAML::Value << entry.cooker_version;
    emitter << YAML::Key << "source_hash" << YAML::Value << entry.source_hash;
    emitter << YAML::Key << "source_mtime" << YAML::Value << entry.source_mtime;
    emitter << YAML::EndMap;
  }

  emitter << YAML::EndSeq;
  emitter << YAML::EndMap;

  auto error = std::error_code{};
  std::filesystem::create_directories(_manifest_path().parent_path(), error);

  auto out = std::ofstream{_manifest_path()};
  out << emitter.c_str();

  _manifest_dirty = false;
}

auto asset_cooker::_cook_texture(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool {
  auto width = std::int32_t{0};
  auto height = std::int32_t{0};
  auto channels = std::int32_t{0};

  auto* data = stbi_load(source.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (data == nullptr) {
    utility::logger<"assets">::warn("Cook: could not decode '{}'", source.generic_string());
    return false;
  }

  const auto data_size = static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height) * 4u;

  const auto header = texture_header{
    texture_magic,
    texture_version,
    static_cast<std::uint32_t>(width),
    static_cast<std::uint32_t>(height),
    4u,
    data_size
  };

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write '{}'", cooked.generic_string());
    stbi_image_free(data);
    return false;
  }

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(data_size));

  stbi_image_free(data);

  utility::logger<"assets">::debug("Cooked texture '{}' -> '{}'", source.generic_string(), cooked.generic_string());

  return true;
}

auto asset_cooker::_load_cooked_texture(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool {
  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = texture_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != texture_magic || header.version != texture_version) {
    return false; // missing / corrupt / stale format -> caller recooks
  }

  pixels.resize(header.data_size);
  in.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(header.data_size));

  if (!in) {
    return false;
  }

  width = header.width;
  height = header.height;

  return true;
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

auto asset_cooker::_cook_mesh(const std::filesystem::path& source, const math::uuid& id, const std::filesystem::path& cooked, const mesh_import_options& options, const material_resolver& resolve_material) -> bool {
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

  const auto texture_uuid = [&](std::size_t texture_index) -> math::uuid {
    const auto& gltf_texture = gltf.textures[texture_index];

    if (!gltf_texture.imageIndex.has_value()) {
      return math::uuid::nil();
    }

    const auto& image = gltf.images[gltf_texture.imageIndex.value()];

    if (const auto* uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
      return import(source.parent_path() / std::filesystem::path{std::string{uri->uri.path()}});
    }

    utility::logger<"assets">::warn("Cook: mesh '{}' has a non-file image, using default", source.generic_string());
    return math::uuid::nil();
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

    if (pbr.baseColorTexture.has_value())         description.albedo             = texture_uuid(pbr.baseColorTexture->textureIndex);
    if (pbr.metallicRoughnessTexture.has_value()) description.metallic_roughness = texture_uuid(pbr.metallicRoughnessTexture->textureIndex);
    if (gltf_material.normalTexture.has_value())  description.normal             = texture_uuid(gltf_material.normalTexture->textureIndex);
    if (gltf_material.occlusionTexture.has_value()) description.occlusion        = texture_uuid(gltf_material.occlusionTexture->textureIndex);
    if (gltf_material.emissiveTexture.has_value()) description.emissive          = texture_uuid(gltf_material.emissiveTexture->textureIndex);

    auto material_uuid = math::uuid::nil();

    if (options.extract_materials) {
      material_uuid = resolve_material(description, source);
    } else {
      material_uuid = _derive_material_uuid(id, material_uuids.size());

      if (!_cook_material(material_uuid, description)) {
        return false;
      }
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

  const auto has_skin_data = !joints.empty();

  const auto append = [&](const fastgltf::Mesh& gltf_mesh, const fastgltf::math::fmat4x4& world, bool is_skinned) {
    for (const auto& primitive : gltf_mesh.primitives) {
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

  auto animation_clip_count = std::uint32_t{0u};

  if (has_skin_data) {
    if (!_cook_skeleton(_derive_skeleton_uuid(id), joints)) {
      return false;
    }

    for (const auto& gltf_animation : gltf.animations) {
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
      clip_data.name = gltf_animation.name.empty() ? fmt::format("clip_{}", animation_clip_count) : std::string{gltf_animation.name.begin(), gltf_animation.name.end()};
      clip_data.duration = duration;
      clip_data.channels = std::move(channels);

      if (!_cook_animation_clip(_derive_animation_clip_uuid(id, animation_clip_count), clip_data)) {
        return false;
      }

      ++animation_clip_count;
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
  header.version = mesh_version;
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
  header.animation_clip_count = animation_clip_count;

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

  if (has_skin_data) {
    utility::logger<"assets">::debug("Cooked mesh '{}' -> '{}' ({} joints, {} animation clips)", source.generic_string(), cooked.generic_string(), joints.size(), animation_clip_count);
  } else {
    utility::logger<"assets">::debug("Cooked mesh '{}' -> '{}'", source.generic_string(), cooked.generic_string());
  }

  return true;
}

auto asset_cooker::_load_cooked_mesh(const std::filesystem::path& cooked, std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<cooked_submesh>& submeshes, math::volume& bounds, std::vector<skin_vertex>& skin_vertices, std::uint32_t& animation_clip_count) -> bool {
  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = mesh_file_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != mesh_magic || header.version != mesh_version) {
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

  animation_clip_count = header.animation_clip_count;

  return true;
}

auto asset_cooker::_cook_material(const math::uuid& id, const material_description& description) -> bool {
  const auto cooked = _cooked_path(id, ".sbxmat");

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write material '{}'", cooked.generic_string());
    return false;
  }

  auto header = material_file_header{};
  header.magic = material_magic;
  header.version = material_version;
  header.base_color_factor[0] = description.base_color_factor.r();
  header.base_color_factor[1] = description.base_color_factor.g();
  header.base_color_factor[2] = description.base_color_factor.b();
  header.base_color_factor[3] = description.base_color_factor.a();
  header.emissive_factor[0] = description.emissive_factor.x();
  header.emissive_factor[1] = description.emissive_factor.y();
  header.emissive_factor[2] = description.emissive_factor.z();
  header.metallic_factor = description.metallic_factor;
  header.roughness_factor = description.roughness_factor;
  header.alpha_mode = static_cast<std::uint32_t>(description.alpha);
  header.alpha_cutoff = description.alpha_cutoff;
  header.is_double_sided = description.is_double_sided ? 1u : 0u;
  header.normal_scale = description.normal_scale;
  header.occlusion_strength = description.occlusion_strength;
  header.emissive_strength = description.emissive_strength;
  header.ior = description.ior;
  header.albedo_uuid = description.albedo.value();
  header.normal_uuid = description.normal.value();
  header.metallic_roughness_uuid = description.metallic_roughness.value();
  header.occlusion_uuid = description.occlusion.value();
  header.emissive_uuid = description.emissive.value();
  header.name_length = static_cast<std::uint32_t>(description.name.size());

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(description.name.data(), static_cast<std::streamsize>(description.name.size()));

  return true;
}

auto asset_cooker::_cook_environment_map(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool {
  auto width = std::int32_t{0};
  auto height = std::int32_t{0};
  auto channels = std::int32_t{0};

  // source (from _paths[uuid]) is already fully resolved — same as _cook_texture's source.string()
  // above; wrapping it in absolute() here would double-prefix assets_directory().
  auto* data = stbi_loadf(source.string().c_str(), &width, &height, &channels, 4);

  if (data == nullptr) {
    utility::logger<"assets">::warn("Cook: could not decode HDR '{}'", source.generic_string());
    return false;
  }

  const auto data_size = static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height) * 4u * static_cast<std::uint32_t>(sizeof(std::float_t));

  const auto header = texture_header{environment_magic, environment_version, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 4u, data_size};

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write '{}'", cooked.generic_string());
    stbi_image_free(data);
    return false;
  }

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(data_size));

  stbi_image_free(data);

  utility::logger<"assets">::debug("Cooked environment '{}' -> '{}'", source.generic_string(), cooked.generic_string());
  return true;
}

auto asset_cooker::_load_cooked_environment_map(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool {
  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = texture_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != environment_magic || header.version != environment_version) {
    return false;
  }

  pixels.resize(header.data_size);
  in.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(header.data_size));

  if (!in) {
    return false;
  }

  width = header.width;
  height = header.height;

  return true;
}

auto asset_cooker::_derive_material_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid {
  // splitmix64 over (mesh uuid, index) — deterministic so re-cooking is stable.
  auto x = mesh.value() ^ (0x9e3779b97f4a7c15ull * (static_cast<std::uint64_t>(index) + 1ull));
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ull;
  x ^= x >> 27; x *= 0x94d049bb133111ebull;
  x ^= x >> 31;

  return math::uuid::from_value(x == 0ull ? 1ull : x); // never nil
}

auto asset_cooker::_derive_skeleton_uuid(const math::uuid& mesh) -> math::uuid {
  // Same splitmix64 shape as _derive_material_uuid, salted differently so a mesh's skeleton uuid
  // never collides with one of its material uuids.
  auto x = mesh.value() ^ 0xff51afd7ed558ccdull;
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ull;
  x ^= x >> 27; x *= 0x94d049bb133111ebull;
  x ^= x >> 31;

  return math::uuid::from_value(x == 0ull ? 1ull : x); // never nil
}

auto asset_cooker::_derive_animation_clip_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid {
  auto x = mesh.value() ^ (0xc2b2ae3d27d4eb4full * (static_cast<std::uint64_t>(index) + 1ull));
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ull;
  x ^= x >> 27; x *= 0x94d049bb133111ebull;
  x ^= x >> 31;

  return math::uuid::from_value(x == 0ull ? 1ull : x); // never nil
}

auto asset_cooker::_cook_skeleton(const math::uuid& id, const std::vector<skeleton::joint>& joints) -> bool {
  const auto cooked = _cooked_path(id, ".sbxskl");

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write skeleton '{}'", cooked.generic_string());
    return false;
  }

  auto header = skeleton_file_header{};
  header.magic = skeleton_magic;
  header.version = skeleton_version;
  header.joint_count = static_cast<std::uint32_t>(joints.size());

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));

  for (const auto& joint : joints) {
    auto record = skeleton_joint_record{};
    record.parent_index = joint.parent_index;

    for (auto column = std::size_t{0u}; column < 4u; ++column) {
      for (auto row = std::size_t{0u}; row < 4u; ++row) {
        record.inverse_bind_matrix[column * 4u + row] = joint.inverse_bind_matrix[column][row];
      }
    }

    record.bind_translation[0] = joint.bind_local_translation.x();
    record.bind_translation[1] = joint.bind_local_translation.y();
    record.bind_translation[2] = joint.bind_local_translation.z();
    record.bind_rotation[0] = joint.bind_local_rotation.x();
    record.bind_rotation[1] = joint.bind_local_rotation.y();
    record.bind_rotation[2] = joint.bind_local_rotation.z();
    record.bind_rotation[3] = joint.bind_local_rotation.w();
    record.bind_scale[0] = joint.bind_local_scale.x();
    record.bind_scale[1] = joint.bind_local_scale.y();
    record.bind_scale[2] = joint.bind_local_scale.z();
    record.name_length = static_cast<std::uint32_t>(joint.name.size());

    out.write(reinterpret_cast<const char*>(&record), sizeof(record));
    out.write(joint.name.data(), static_cast<std::streamsize>(joint.name.size()));
  }

  return true;
}

auto asset_cooker::_load_cooked_skeleton(const math::uuid& id, std::vector<skeleton::joint>& joints) -> bool {
  const auto cooked = _cooked_path(id, ".sbxskl");

  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = skeleton_file_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != skeleton_magic || header.version != skeleton_version) {
    utility::logger<"assets">::warn("Invalid cooked skeleton '{}'", cooked.generic_string());
    return false;
  }

  joints.clear();
  joints.reserve(header.joint_count);

  for (auto i = std::uint32_t{0u}; i < header.joint_count; ++i) {
    auto record = skeleton_joint_record{};
    in.read(reinterpret_cast<char*>(&record), sizeof(record));

    if (!in) {
      return false;
    }

    auto name = std::string(record.name_length, '\0');

    if (record.name_length > 0u) {
      in.read(name.data(), static_cast<std::streamsize>(record.name_length));

      if (!in) {
        return false;
      }
    }

    auto joint = skeleton::joint{};
    joint.name = std::move(name);
    joint.parent_index = record.parent_index;

    for (auto column = std::size_t{0u}; column < 4u; ++column) {
      for (auto row = std::size_t{0u}; row < 4u; ++row) {
        joint.inverse_bind_matrix[column][row] = record.inverse_bind_matrix[column * 4u + row];
      }
    }

    joint.bind_local_translation = math::vector3{record.bind_translation[0], record.bind_translation[1], record.bind_translation[2]};
    joint.bind_local_rotation = math::quaternion::wxyz(record.bind_rotation[3], record.bind_rotation[0], record.bind_rotation[1], record.bind_rotation[2]);
    joint.bind_local_scale = math::vector3{record.bind_scale[0], record.bind_scale[1], record.bind_scale[2]};

    joints.push_back(std::move(joint));
  }

  return true;
}

auto asset_cooker::_cook_animation_clip(const math::uuid& id, const animation_clip_data& data) -> bool {
  const auto cooked = _cooked_path(id, ".sbxanm");

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write animation clip '{}'", cooked.generic_string());
    return false;
  }

  auto header = animation_clip_file_header{};
  header.magic = animation_magic;
  header.version = animation_version;
  header.duration = data.duration;
  header.channel_count = static_cast<std::uint32_t>(data.channels.size());
  header.name_length = static_cast<std::uint32_t>(data.name.size());

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(data.name.data(), static_cast<std::streamsize>(data.name.size()));

  for (const auto& channel : data.channels) {
    auto record = animation_channel_record{};
    record.joint_index = channel.joint_index;
    record.translation_key_count = static_cast<std::uint32_t>(channel.translation_keys.size());
    record.rotation_key_count = static_cast<std::uint32_t>(channel.rotation_keys.size());
    record.scale_key_count = static_cast<std::uint32_t>(channel.scale_keys.size());
    record.translation_interpolation = static_cast<std::uint32_t>(channel.translation_interpolation);
    record.rotation_interpolation = static_cast<std::uint32_t>(channel.rotation_interpolation);
    record.scale_interpolation = static_cast<std::uint32_t>(channel.scale_interpolation);

    out.write(reinterpret_cast<const char*>(&record), sizeof(record));

    for (const auto& key : channel.translation_keys) {
      const auto key_record = vector3_key_record{key.time, {key.value.x(), key.value.y(), key.value.z()}};
      out.write(reinterpret_cast<const char*>(&key_record), sizeof(key_record));
    }

    for (const auto& key : channel.rotation_keys) {
      const auto key_record = quaternion_key_record{key.time, {key.value.x(), key.value.y(), key.value.z(), key.value.w()}};
      out.write(reinterpret_cast<const char*>(&key_record), sizeof(key_record));
    }

    for (const auto& key : channel.scale_keys) {
      const auto key_record = vector3_key_record{key.time, {key.value.x(), key.value.y(), key.value.z()}};
      out.write(reinterpret_cast<const char*>(&key_record), sizeof(key_record));
    }
  }

  return true;
}

auto asset_cooker::_load_cooked_animation_clip(const math::uuid& id, animation_clip_data& data) -> bool {
  const auto cooked = _cooked_path(id, ".sbxanm");

  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = animation_clip_file_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != animation_magic || header.version != animation_version) {
    utility::logger<"assets">::warn("Invalid cooked animation clip '{}'", cooked.generic_string());
    return false;
  }

  auto name = std::string(header.name_length, '\0');

  if (header.name_length > 0u) {
    in.read(name.data(), static_cast<std::streamsize>(header.name_length));

    if (!in) {
      return false;
    }
  }

  data.name = std::move(name);
  data.duration = header.duration;
  data.channels.clear();
  data.channels.reserve(header.channel_count);

  for (auto i = std::uint32_t{0u}; i < header.channel_count; ++i) {
    auto record = animation_channel_record{};
    in.read(reinterpret_cast<char*>(&record), sizeof(record));

    if (!in) {
      return false;
    }

    auto channel = animation_joint_channel{};
    channel.joint_index = record.joint_index;
    channel.translation_interpolation = static_cast<animation_interpolation>(record.translation_interpolation);
    channel.rotation_interpolation = static_cast<animation_interpolation>(record.rotation_interpolation);
    channel.scale_interpolation = static_cast<animation_interpolation>(record.scale_interpolation);

    channel.translation_keys.reserve(record.translation_key_count);

    for (auto k = std::uint32_t{0u}; k < record.translation_key_count; ++k) {
      auto key_record = vector3_key_record{};
      in.read(reinterpret_cast<char*>(&key_record), sizeof(key_record));

      if (!in) {
        return false;
      }

      channel.translation_keys.push_back({key_record.time, math::vector3{key_record.value[0], key_record.value[1], key_record.value[2]}});
    }

    channel.rotation_keys.reserve(record.rotation_key_count);

    for (auto k = std::uint32_t{0u}; k < record.rotation_key_count; ++k) {
      auto key_record = quaternion_key_record{};
      in.read(reinterpret_cast<char*>(&key_record), sizeof(key_record));

      if (!in) {
        return false;
      }

      channel.rotation_keys.push_back({key_record.time, math::quaternion::wxyz(key_record.value[3], key_record.value[0], key_record.value[1], key_record.value[2])});
    }

    channel.scale_keys.reserve(record.scale_key_count);

    for (auto k = std::uint32_t{0u}; k < record.scale_key_count; ++k) {
      auto key_record = vector3_key_record{};
      in.read(reinterpret_cast<char*>(&key_record), sizeof(key_record));

      if (!in) {
        return false;
      }

      channel.scale_keys.push_back({key_record.time, math::vector3{key_record.value[0], key_record.value[1], key_record.value[2]}});
    }

    data.channels.push_back(std::move(channel));
  }

  return true;
}

} // namespace sbx::assets
