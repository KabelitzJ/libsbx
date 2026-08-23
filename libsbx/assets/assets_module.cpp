// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/assets_module.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

#include <yaml-cpp/yaml.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/math.hpp>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/fourcc.hpp>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/timer.hpp>

#include <libsbx/math/vector3.hpp>

#include <libsbx/graphics/resources/sampler.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/shader_cache.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline_cache.hpp>

namespace sbx::assets {

inline constexpr auto material_flag_masked = std::uint32_t{1u << 0u};

struct material_data {
  math::vector4 base_color_factor;
  math::vector4 emissive_factor;
  std::uint32_t albedo_index;
  std::uint32_t normal_index;
  std::uint32_t metallic_roughness_index;
  std::uint32_t occlusion_index;
  std::uint32_t emissive_index;
  std::float_t metallic_factor;
  std::float_t roughness_factor;
  std::float_t alpha_cutoff;
  std::uint32_t flags;
  std::uint32_t padding0;
  std::uint32_t padding1;
  std::uint32_t padding2;
}; // struct material_data

inline constexpr auto texture_magic = utility::fourcc_v<"SBTX">;  // 'SBTX'
inline constexpr auto texture_version = std::uint32_t{1u};

inline constexpr auto mesh_magic = utility::fourcc_v<"SBSH">;   // 'SBSH'
inline constexpr auto mesh_version = std::uint32_t{5u}; // bumped again: generate fallback tangents for primitives with no TANGENT attribute (fixes NaN-shading-normal -> whole mesh renders white)

inline constexpr auto material_magic = utility::fourcc_v<"SBMT">; // 'SBMT'
inline constexpr auto material_version = std::uint32_t{2u};

inline constexpr auto environment_magic = utility::fourcc_v<"SBEN">; // 'SBEN'
inline constexpr auto environment_version = std::uint32_t{1u};

// A mesh cook also emits its materials, so a mesh blob's freshness depends on both cookers.
inline constexpr auto mesh_cooker_version = mesh_version * 1000u + material_version;

// Strips characters a filename can't contain, for turning a gltf material's (freeform) name into
// a safe file name when extracting it.
static auto sanitize_file_name(std::string name) -> std::string {
  for (auto& character : name) {
    if (character == '/' || character == '\\' || character == ':' || character == '*' || character == '?' || character == '"' || character == '<' || character == '>' || character == '|') {
      character = '_';
    }
  }

  return name;
}

// FNV-1a over the file's bytes. Returns 0 on failure (treated as "changed" -> recook).
static auto hash_file(const std::filesystem::path& path) -> std::uint64_t {
  auto in = std::ifstream{path, std::ios::binary};

  if (!in) {
    return 0u;
  }

  auto hash = std::uint64_t{14695981039346656037ull};
  auto buffer = std::array<char, std::size_t{1u} << 16>{};

  while (in) {
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = static_cast<std::size_t>(in.gcount());

    for (auto i = std::size_t{0u}; i < count; ++i) {
      hash ^= static_cast<std::uint8_t>(buffer[i]);
      hash *= 1099511628211ull;
    }
  }

  return hash;
}

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
  std::uint32_t vertex_count;
  std::uint32_t index_count;
  std::uint32_t submesh_count;
  std::float_t bounds_min[3];
  std::float_t bounds_max[3];
}; // struct mesh_file_header

struct submesh_file_record {
  std::uint32_t index_offset;
  std::uint32_t index_count;
  std::float_t bounds_min[3];
  std::float_t bounds_max[3];
  std::uint64_t material_uuid; // 0 = none
}; // struct submesh_file_record

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
  std::uint64_t albedo_uuid;
  std::uint64_t normal_uuid;
  std::uint64_t metallic_roughness_uuid;
  std::uint64_t occlusion_uuid;
  std::uint64_t emissive_uuid;
  std::uint32_t name_length;
}; // struct material_file_header

assets_module::assets_module() {
  _white = _create_default_texture({255u, 255u, 255u, 255u});
  _normal = _create_default_texture({128u, 128u, 255u, 255u}); // (0,0,1) tangent-space normal
  _black = _create_default_texture({0u, 0u, 0u, 255u});
  _magenta = _create_default_texture({255u, 0u, 255u, 255u});   // load-error marker
}

assets_module::~assets_module() {
  _save_manifest();
}

auto assets_module::import(const std::filesystem::path& path) -> math::uuid {
  _ensure_manifest_loaded();

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

auto assets_module::import_directory(const std::filesystem::path& root) -> void {
  _ensure_manifest_loaded();

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

    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".gltf" || extension == ".glb" || extension == ".material" || extension == ".hdr") {
      import(entry.path());
    }
  }
}

auto assets_module::load_texture(const math::uuid& id, graphics::format format) -> texture_handle {
  _ensure_manifest_loaded();

  const auto is_srgb = (format == graphics::format::r8g8b8a8_srgb);

  const auto key = fmt::format("{}:{}", id.value(), (is_srgb ? "#srgb" : "#linear"));

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _textures.find(key); entry != _textures.end()) {
      return texture_handle{entry->second};
    }
  }

  auto path = std::filesystem::path{};
  {
    auto lock = std::lock_guard{_mutex};

    const auto entry = _paths.find(id);

    if (entry == _paths.end()) {
      utility::logger<"assets">::warn("Unknown texture uuid {}", id);
      return texture_handle{};
    }

    path = entry->second;
  }

  const auto cooked = _cooked_path(id, ".sbxtex");

  if (_is_cooked_stale(id, path, cooked, texture_version)) {
    if (!_cook_texture(path, cooked)) {
      utility::logger<"assets">::warn("Could not cook texture '{}'", path.generic_string());
      return texture_handle{};
    }
    _record_cook(id, texture_version, path);
  }

  auto pixels = std::vector<std::byte>{};
  auto width = std::uint32_t{0u};
  auto height = std::uint32_t{0u};

  // If the blob is unreadable/out-of-date (e.g. cooker version bumped), recook once.
  if (!_load_cooked_texture(cooked, pixels, width, height)) {
    if (!_cook_texture(path, cooked) || !_load_cooked_texture(cooked, pixels, width, height)) {
      utility::logger<"assets">::warn("Could not load cooked texture '{}'", cooked.generic_string());
      return texture_handle{};
    }
    _record_cook(id, texture_version, path);
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& bindless_table = graphics_module.bindless_table();

  const auto index = bindless_table.reserve_sampled_image();

  auto record = std::make_shared<texture>(texture{index});
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};

    _textures.emplace(key, record);
    _pending_textures.push_back(pending_texture_upload{index, std::move(pixels), width, height, format});
  }

  return texture_handle{record};
}

auto assets_module::load_texture(const std::filesystem::path& path, graphics::format format) -> texture_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_texture(import(assets_directory / path), format);
}

auto assets_module::load_mesh(const math::uuid& id, const mesh_import_options& options) -> mesh_handle {
  _ensure_manifest_loaded();

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _meshes.find(id); entry != _meshes.end()) {
      return mesh_handle{entry->second};
    }
  }

  auto relative_path = std::filesystem::path{};
  {
    auto lock = std::lock_guard{_mutex};

    const auto entry = _paths.find(id);

    if (entry == _paths.end()) {
      utility::logger<"assets">::warn("Unknown mesh uuid {}", id);

      return mesh_handle{};
    }

    relative_path = entry->second;
  }

  const auto source = relative_path;
  const auto cooked = _cooked_path(id, ".sbxmsh");

  if (_is_cooked_stale(id, source, cooked, mesh_cooker_version)) {
    if (!_cook_mesh(source, relative_path, id, cooked, options)) {
      return mesh_handle{};
    }
    _record_cook(id, mesh_cooker_version, source);
  }

  auto vertices = std::vector<vertex>{};
  auto indices = std::vector<std::uint32_t>{};
  auto cooked_submeshes = std::vector<cooked_submesh>{};
  auto bounds = math::volume{};

  if (!_load_cooked_mesh(cooked, vertices, indices, cooked_submeshes, bounds)) {
    if (!_cook_mesh(source, relative_path, id, cooked, options) || !_load_cooked_mesh(cooked, vertices, indices, cooked_submeshes, bounds)) {
      utility::logger<"assets">::warn("Could not load cooked mesh '{}'", cooked.generic_string());

      return mesh_handle{};
    }
    _record_cook(id, mesh_cooker_version, source);
  }

  if (vertices.empty() || indices.empty()) {
    utility::logger<"assets">::warn("Mesh '{}' has no drawable geometry", source.generic_string());

    return mesh_handle{};
  }

  auto fallback_material = material_handle{};

  auto submeshes = std::vector<mesh::submesh>{};
  submeshes.reserve(cooked_submeshes.size());

  for (const auto& cooked_submesh : cooked_submeshes) {
    auto material = material_handle{};

    if (cooked_submesh.material != math::uuid::nil()) {
      material = load_material(cooked_submesh.material);
    }

    if (!material.is_valid()) {
      if (!fallback_material.is_valid()) {
        fallback_material = create_material(material::create_info{ .albedo = _magenta });
      }
      material = fallback_material;
    }

    submeshes.push_back(mesh::submesh{cooked_submesh.index_offset, cooked_submesh.index_count, cooked_submesh.bounds, material});
  }

  const auto vertex_count = vertices.size();
  const auto index_count = indices.size();
  const auto submesh_count = submeshes.size();

  auto record = std::make_shared<mesh>(std::move(submeshes), bounds);
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};

    _meshes.emplace(id, record);
    _pending_meshes.push_back(pending_mesh_upload{record, std::move(vertices), std::move(indices)});
  }

  utility::logger<"assets">::info("Loaded mesh '{}': {} vertices, {} indices, {} submeshes", source.generic_string(), vertex_count, index_count, submesh_count);

  return mesh_handle{record};
}

auto assets_module::load_mesh(const std::filesystem::path& path, const mesh_import_options& options) -> mesh_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_mesh(import(assets_directory / path), options);
}

auto assets_module::load_material(const math::uuid& id) -> material_handle {
  _ensure_manifest_loaded();

  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _material_files.find(id); entry != _material_files.end()) {
      return material_handle{entry->second};
    }
  }

  auto source_path = std::filesystem::path{};
  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _paths.find(id); entry != _paths.end()) {
      source_path = entry->second;
    }
  }

  if (!source_path.empty() && source_path.extension() == ".material") {
    const auto path = source_path;

    auto root = YAML::Node{};
    try {
      root = YAML::LoadFile(path.string());
    } catch (const std::exception& exception) {
      utility::logger<"assets">::warn("Could not parse material '{}' ({})", path.generic_string(), exception.what());
      return material_handle{};
    }

    auto info = material::create_info{};

    if (root["name"]) info.name = root["name"].as<std::string>();
    if (root["base_color_factor"]) info.base_color_factor = root["base_color_factor"].as<math::color>();
    if (root["emissive_factor"]) info.emissive_factor = root["emissive_factor"].as<math::vector3>();
    if (root["metallic_factor"]) info.metallic_factor = root["metallic_factor"].as<std::float_t>();
    if (root["roughness_factor"]) info.roughness_factor = root["roughness_factor"].as<std::float_t>();
    if (root["alpha_mode"]) {
      const auto mode = root["alpha_mode"].as<std::string>();
      info.alpha = (mode == "blend") ? alpha_mode::blend : (mode == "mask") ? alpha_mode::mask : alpha_mode::opaque;
    }
    if (root["alpha_cutoff"]) info.alpha_cutoff = root["alpha_cutoff"].as<std::float_t>();
    if (root["is_double_sided"]) info.is_double_sided = root["is_double_sided"].as<bool>();

    const auto load_slot = [&](const char* key, graphics::format format) -> texture_handle {
      if (const auto node = root[key]) {
        return load_texture(std::filesystem::path{node.as<std::string>()}, format);
      }
      return texture_handle{};
    };

    info.albedo = load_slot("albedo", graphics::format::r8g8b8a8_srgb);
    info.normal = load_slot("normal", graphics::format::r8g8b8a8_unorm);
    info.metallic_roughness = load_slot("metallic_roughness", graphics::format::r8g8b8a8_unorm);
    info.occlusion = load_slot("occlusion", graphics::format::r8g8b8a8_unorm);
    info.emissive = load_slot("emissive", graphics::format::r8g8b8a8_srgb);

    auto record = std::make_shared<material>(info);
    record->_id = id;

    auto handle = _register_material(record);

    {
      auto lock = std::lock_guard{_mutex};
      _material_files.emplace(id, record);
    }

    utility::logger<"assets">::info("Loaded material '{}'", path.generic_string());

    return handle;
  }

  // Otherwise a cooked material extracted from a mesh import.
  const auto cooked = _cooked_path(id, ".sbxmat");

  if (std::filesystem::exists(cooked)) {
    return _load_cooked_material(cooked, id);
  }

  utility::logger<"assets">::warn("Unknown material uuid {}", id);

  return material_handle{};
}


auto assets_module::load_material(const std::filesystem::path& path) -> material_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_material(import(assets_directory / path));
}

auto assets_module::create_material(const material::create_info& create_info) -> material_handle {
  return _register_material(std::make_shared<material>(create_info));
}

auto assets_module::update_material(material_handle& material, const material::create_info& create_info) -> void {
  if (!material.is_valid()) {
    return;
  }

  material->_base_color_factor = create_info.base_color_factor;
  material->_emissive_factor = create_info.emissive_factor;
  material->_metallic_factor = create_info.metallic_factor;
  material->_roughness_factor = create_info.roughness_factor;
  material->_alpha = create_info.alpha;
  material->_alpha_cutoff = create_info.alpha_cutoff;
  material->_is_double_sided = create_info.is_double_sided;
  material->_albedo = create_info.albedo;
  material->_normal = create_info.normal;
  material->_metallic_roughness = create_info.metallic_roughness;
  material->_occlusion = create_info.occlusion;
  material->_emissive = create_info.emissive;
  material->_name = create_info.name;

  // _register_material only ever queues a GPU buffer upload once, at creation time — without
  // re-queuing here, an in-place edit updates the CPU-side object but the renderer keeps reading
  // the stale material_data it already uploaded. _materials is indexed by material::index(), the
  // same slot _register_material itself pushed to, so this is the same shared_ptr, not a copy.
  auto lock = std::lock_guard{_mutex};

  if (material->index() < _materials.size()) {
    _pending_materials.push_back(pending_material_upload{_materials[material->index()]});
  }
}

auto assets_module::save_material(material_handle& material, const std::filesystem::path& path) -> math::uuid {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  const auto resolved_path = assets_directory / path;

  if (!material.is_valid()) {
    utility::logger<"assets">::warn("Cannot save an invalid material to '{}'", resolved_path.generic_string());
    return math::uuid::nil();
  }

  const auto path_of = [this](const texture_handle& texture) -> std::optional<std::string> {
    if (!texture.is_valid()) {
      return std::nullopt;
    }

    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _paths.find(texture->id()); entry != _paths.end()) {
      // entry->second is stored fully resolved; the slot needs to hold the assets-relative form
      // (that's what load_material's own reader passes straight into load_texture(path, ...)).
      return _relative(entry->second).generic_string();
    }

    return std::nullopt; // default/procedural texture (nil uuid) — omit the slot
  };

  auto node = YAML::Node{};

  node["name"] = material->name();
  node["base_color_factor"] = material->base_color_factor();
  node["emissive_factor"] = material->emissive_factor();
  node["metallic_factor"] = material->metallic_factor();
  node["roughness_factor"] = material->roughness_factor();
  node["alpha_mode"] = (material->alpha() == alpha_mode::blend) ? "blend" : (material->alpha() == alpha_mode::mask) ? "mask" : "opaque";
  node["alpha_cutoff"] = material->alpha_cutoff();
  node["is_double_sided"] = material->is_double_sided();

  if (const auto slot = path_of(material->albedo())) {
    node["albedo"] = *slot;
  }

  if (const auto slot = path_of(material->normal())) {
    node["normal"] = *slot;
  }

  if (const auto slot = path_of(material->metallic_roughness())) {
    node["metallic_roughness"] = *slot;
  }

  if (const auto slot = path_of(material->occlusion())) {
    node["occlusion"] = *slot;
  }

  if (const auto slot = path_of(material->emissive())) {
    node["emissive"] = *slot;
  }

  if (!resolved_path.parent_path().empty()) {
    std::filesystem::create_directories(resolved_path.parent_path());
  }

  auto out = std::ofstream{resolved_path};
  out << node;

  const auto id = import(resolved_path); // register + create the .meta so it's a first-class asset

  // import() is idempotent (returns the existing uuid from .meta on a re-save), so this is always
  // the right id to stamp onto the record — including the very first save of a create_material()'d
  // material, which otherwise keeps a nil id forever.
  material->_id = id;

  utility::logger<"assets">::info("Saved material '{}'", resolved_path.generic_string());

  return id;
}

auto assets_module::load_environment_map(const math::uuid& id) -> environment_map_handle {
  auto timer = utility::scoped_timer{[&id](const units::seconds& elapsed) {
    utility::logger<"assets">::info("Loaded environment map {} in {}", id, units::milliseconds{elapsed});
  }};

  _ensure_manifest_loaded();

  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _environment_maps.find(id); entry != _environment_maps.end()) {
      return environment_map_handle{entry->second};
    }
  }

  auto relative_path = std::filesystem::path{};
  {
    auto lock = std::lock_guard{_mutex};
    const auto entry = _paths.find(id);
    if (entry == _paths.end()) {
      utility::logger<"assets">::warn("Unknown environment map uuid {}", id);
      return environment_map_handle{};
    }
    relative_path = entry->second;
  }

  const auto source = relative_path;
  const auto cooked = _cooked_path(id, ".sbxenv");

  if (_is_cooked_stale(id, source, cooked, environment_version)) {
    if (!_cook_environment_map(source, cooked)) {
      utility::logger<"assets">::warn("Could not cook environment map '{}'", source.generic_string());
      return environment_map_handle{};
    }
    _record_cook(id, environment_version, source);
  }

  auto pixels = std::vector<std::byte>{};
  auto width = std::uint32_t{0u};
  auto height = std::uint32_t{0u};

  if (!_load_cooked_environment_map(cooked, pixels, width, height)) {
    if (!_cook_environment_map(source, cooked) || !_load_cooked_environment_map(cooked, pixels, width, height)) {
      utility::logger<"assets">::warn("Could not load cooked environment map '{}'", cooked.generic_string());
      return environment_map_handle{};
    }
    _record_cook(id, environment_version, source);
  }

  auto record = std::make_shared<environment_map>();
  record->_id = id;

  // Bakes irradiance + prefiltered via compute at load time and blocks until the GPU has
  // finished — the environment is fully usable the moment this call returns, matching the old
  // engine's load-time bake semantics rather than the render thread's old lazy first-frame bake.
  _bake_environment(*record, pixels, width, height);

  {
    auto lock = std::lock_guard{_mutex};
    _environment_maps.emplace(id, record);
  }

  return environment_map_handle{record};
}

auto assets_module::load_environment_map(const std::filesystem::path& path) -> environment_map_handle {
  const auto& project = core::engine::project();

  const auto assets_directory = project.assets_directory();

  return load_environment_map(import(assets_directory / path));
}

auto assets_module::process_uploads(std::uint64_t frame_index) -> void {
  auto pending_textures = std::vector<pending_texture_upload>{};
  auto pending_meshes = std::vector<pending_mesh_upload>{};
  auto pending_materials = std::vector<pending_material_upload>{};

  {
    auto lock = std::lock_guard{_mutex};
    pending_textures.swap(_pending_textures);
    pending_meshes.swap(_pending_meshes);
    pending_materials.swap(_pending_materials);
  }

  if (pending_textures.empty() && pending_meshes.empty() && pending_materials.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& upload_context = graphics_module.upload_context();
  auto& bindless_table = graphics_module.bindless_table();

  for (auto& request : pending_textures) {
    const auto mip_levels = graphics::image::mip_levels_for(math::vector3u{request.width, request.height, 1u});

    const auto handle = registry.emplace<graphics::image>(graphics::image::create_info{
      .extent = math::vector3u{request.width, request.height, 1u},
      .format = request.format,
      .usage = graphics::image_usage::transfer_destination | graphics::image_usage::transfer_source | graphics::image_usage::sampled,
      .mip_levels = mip_levels,
      .name = "Texture"
    });

    const auto bytes = std::span<const std::byte>{request.pixels.data(), request.pixels.size()};

    upload_context.stage_image(handle, bytes, graphics::image_layout::shader_read_only_optimal);

    bindless_table.write_sampled_image(request.index, registry.get<graphics::image>(handle).view());

    _images.emplace(request.index, handle);
    _resident_frame.emplace(request.index, frame_index);
  }

  for (auto& request : pending_meshes) {
    const auto vertex_bytes = static_cast<graphics::buffer::size_type>(request.vertices.size() * sizeof(vertex));
    const auto index_bytes = static_cast<graphics::buffer::size_type>(request.indices.size() * sizeof(std::uint32_t));

    const auto vertex_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = vertex_bytes,
      .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::transfer_destination,
      .memory = graphics::memory_usage::device_local,
      .name = "Mesh Vertices"
    });

    const auto index_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = index_bytes,
      .usage = graphics::buffer_usage::index | graphics::buffer_usage::transfer_destination,
      .memory = graphics::memory_usage::device_local,
      .name = "Mesh Indices"
    });

    upload_context.stage_buffer(vertex_buffer, std::as_bytes(std::span{request.vertices}));
    upload_context.stage_buffer(index_buffer, std::as_bytes(std::span{request.indices}));

    const auto vertex_address = registry.get<graphics::buffer>(vertex_buffer).address();

    request.record->_finalize(vertex_buffer, index_buffer, vertex_address, frame_index);
  }

  // Create the material buffer once, sized for the whole capacity.
  if (!_material_buffer.is_valid()) {
    _material_buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = material_capacity * memory::stride_v<material_data>,
      .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
      .memory = graphics::memory_usage::host_write,
      .name = "Material Data"
    });

    _material_address = registry.get<graphics::buffer>(_material_buffer).address();
  }

  auto& buffer = registry.get<graphics::buffer>(_material_buffer);

  for (auto& request : pending_materials) {
    const auto& material = *request.record;

    const auto resolve = [this](const texture_handle& texture, const texture_handle& fallback) {
      return texture.is_valid() ? texture->index() : fallback->index();
    };

    const auto& base_color_factor = material.base_color_factor();
    const auto& emissive_factor = material.emissive_factor();

    auto data = material_data{};
    data.base_color_factor = math::vector4{base_color_factor.r(), base_color_factor.g(), base_color_factor.b(), base_color_factor.a()};
    data.emissive_factor = math::vector4{emissive_factor.x(), emissive_factor.y(), emissive_factor.z(), 0.0f};
    data.albedo_index = resolve(material.albedo(), _white);
    data.normal_index = resolve(material.normal(), _normal);
    data.metallic_roughness_index = resolve(material.metallic_roughness(), _white);
    data.occlusion_index = resolve(material.occlusion(), _white);
    data.emissive_index = resolve(material.emissive(), _black);
    data.metallic_factor = material.metallic_factor();
    data.roughness_factor = material.roughness_factor();
    data.alpha_cutoff = material.alpha_cutoff();
    data.flags = (material.alpha() == alpha_mode::mask) ? material_flag_masked : 0u;

    buffer.write(&data, sizeof(material_data), material.index() * memory::stride_v<material_data>);
  }
}

auto assets_module::is_resident(const texture_handle& texture) const -> bool {
  if (!texture.is_valid()) {
    return false;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& frame_context = graphics_module.frame_context();

  const auto completed_value = frame_context.timeline_value();

  auto lock = std::lock_guard{_mutex};

  const auto entry = _resident_frame.find(texture->index());

  return entry != _resident_frame.end() && completed_value >= entry->second;
}

auto assets_module::is_resident(const mesh_handle& mesh) const -> bool {
  if (!mesh.is_valid() || !mesh->is_uploaded()) {
    return false;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto value = graphics_module.frame_context().timeline_value();

  return value >= mesh->resident_frame();
}

auto assets_module::is_resident(const material_handle& material) const -> bool {
  if (!material.is_valid()) {
    return false;
  }

  const auto is_ready = [this](const texture_handle& texture) -> bool {
    return !texture.is_valid() || is_resident(texture);
  };

  return is_ready(material->albedo()) && is_ready(material->normal()) && is_ready(material->metallic_roughness()) && is_ready(material->occlusion()) && is_ready(material->emissive());
}

auto assets_module::is_resident(const environment_map_handle& environment) const -> bool {
  // _bake_environment blocks until the GPU has finished, so a valid handle is always fully baked
  // and resident by the time load_environment_map returns it — no timeline wait needed here,
  // unlike textures/meshes/materials which still upload through the deferred per-frame path.
  return environment.is_valid();
}

auto assets_module::path_of(const math::uuid& id) const -> std::filesystem::path {
  auto lock = std::lock_guard{_mutex};

  if (const auto entry = _paths.find(id); entry != _paths.end()) {
    return entry->second;
  }

  return {};
}

auto assets_module::_create_default_texture(std::array<std::uint8_t, 4u> color) -> texture_handle {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto index = graphics_module.bindless_table().reserve_sampled_image();

  auto record = std::make_shared<texture>(texture{index});

  auto pixels = std::vector<std::byte>{
    std::byte{color[0]}, std::byte{color[1]}, std::byte{color[2]}, std::byte{color[3]}
  };

  {
    auto lock = std::lock_guard{_mutex};
    _pending_textures.push_back(pending_texture_upload{index, std::move(pixels), 1u, 1u, graphics::format::r8g8b8a8_unorm});
  }

  return texture_handle{record};
}

auto assets_module::_read_or_create_meta(const std::filesystem::path& path) -> math::uuid {
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

auto assets_module::_register_material(std::shared_ptr<material> record) -> material_handle {
  auto lock = std::lock_guard{_mutex};

  utility::assert_that(_material_count < material_capacity, "Exceeded material capacity");

  record->_index = _material_count++;

  _materials.push_back(record);
  _pending_materials.push_back(pending_material_upload{record});

  return material_handle{record};
}

auto assets_module::_absolute(const std::filesystem::path& relative) -> std::filesystem::path {
  const auto& project = core::engine::project();

  return project.assets_directory() / relative;
}

// The inverse of _absolute — an already-resolved (cwd-openable) path back to one relative to
// assets_directory(), for writing into a place (like a .material file's texture slots) that's
// meant to hold the assets-relative form. _paths entries are stored fully resolved (see
// assets_module.hpp's class doc comment), so anything read from _paths needs this before being
// handed to something that expects assets-relative input (load_*(path), save_material's own path
// argument, ...).
auto assets_module::_relative(const std::filesystem::path& absolute) -> std::filesystem::path {
  const auto& project = core::engine::project();

  return std::filesystem::relative(absolute, project.assets_directory());
}

auto assets_module::_cooked_path(const math::uuid& id, std::string_view extension) const -> std::filesystem::path {
  const auto& project = core::engine::project();

  return project.library_directory() / fmt::format("{}{}", id.value(), extension);
}

auto assets_module::_is_cooked_stale(const math::uuid& id, const std::filesystem::path& source, const std::filesystem::path& cooked, std::uint32_t cooker_version) -> bool {
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

  const auto mtime_count = static_cast<std::int64_t>(mtime.time_since_epoch().count());

  if (entry->second.source_mtime == mtime_count) {
    return false; // fast path: unchanged since last cook
  }

  // mtime moved — confirm with a content hash before recooking.
  if (entry->second.source_hash == hash_file(source)) {
    entry->second.source_mtime = mtime_count; // touched, not changed
    _manifest_dirty = true;
    return false;
  }

  return true;
}

auto assets_module::_record_cook(const math::uuid& id, std::uint32_t cooker_version, const std::filesystem::path& source) -> void {
  auto error = std::error_code{};
  const auto mtime = std::filesystem::last_write_time(source, error);
  const auto mtime_count = error ? std::int64_t{0} : static_cast<std::int64_t>(mtime.time_since_epoch().count());
  const auto hash = hash_file(source);

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

auto assets_module::_ensure_manifest_loaded() -> void {
  {
    auto lock = std::lock_guard{_mutex};

    if (_manifest_loaded) {
      return;
    }

    _manifest_loaded = true;
  }

  _load_manifest();
}

auto assets_module::_manifest_path() const -> std::filesystem::path {
  return core::engine::project().library_directory() / "manifest.yaml";
}

auto assets_module::_load_manifest() -> void {
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

auto assets_module::_save_manifest() -> void {
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

auto assets_module::_cook_texture(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool {
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

auto assets_module::_load_cooked_texture(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool {
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

auto assets_module::_extract_gltf_material(const material_description& description, const std::filesystem::path& relative_source) -> math::uuid {
  // relative_source (== _cook_mesh's `source`, i.e. _paths[mesh_id]) is already fully resolved —
  // cwd-openable, with assets_directory() baked in (same convention save_material/load_material's
  // path overload assume on their *output* side) — but save_material/load_material(path) both
  // expect a path relative to assets_directory() as *input*, so it has to be re-relativized before
  // use here, exactly like the editor's extract_material_to_asset does for the same reason.
  const auto& project = core::engine::project();
  const auto source_relative = std::filesystem::relative(relative_source, project.assets_directory());

  const auto directory = source_relative.parent_path() / "materials"; // mirrors textures already landing in models/<name>/textures/
  const auto relative_path = directory / (sanitize_file_name(description.name.empty() ? "material" : description.name) + ".material");

  // Already extracted (possibly hand-edited since a previous cook) — reuse it as-is, never overwrite.
  if (std::filesystem::exists(_absolute(relative_path))) {
    if (auto existing = load_material(relative_path); existing.is_valid()) {
      return existing->id();
    }
  }

  auto info = material::create_info{};
  info.name = description.name.empty() ? "material" : description.name;
  info.base_color_factor = description.base_color_factor;
  info.emissive_factor = description.emissive_factor;
  info.metallic_factor = description.metallic_factor;
  info.roughness_factor = description.roughness_factor;
  info.alpha = description.alpha;
  info.alpha_cutoff = description.alpha_cutoff;
  info.is_double_sided = description.is_double_sided;

  const auto load_slot = [&](const math::uuid& uuid, graphics::format format) -> texture_handle {
    return (uuid == math::uuid::nil()) ? texture_handle{} : load_texture(uuid, format);
  };

  info.albedo = load_slot(description.albedo, graphics::format::r8g8b8a8_srgb);
  info.normal = load_slot(description.normal, graphics::format::r8g8b8a8_unorm);
  info.metallic_roughness = load_slot(description.metallic_roughness, graphics::format::r8g8b8a8_unorm);
  info.occlusion = load_slot(description.occlusion, graphics::format::r8g8b8a8_unorm);
  info.emissive = load_slot(description.emissive, graphics::format::r8g8b8a8_srgb);

  auto handle = create_material(info);

  return save_material(handle, relative_path);
}

auto assets_module::_generate_tangents(std::vector<vertex>& vertices, const std::vector<std::uint32_t>& indices, std::size_t vertex_start, std::size_t vertex_count, std::size_t index_start, std::size_t index_count) -> void {
  // Lengyel's per-triangle method: accumulate a tangent and bitangent per vertex from every
  // triangle referencing it, then orthogonalize against the (already-populated) normal and derive
  // handedness from the accumulated bitangent. Standard fallback used by essentially every glTF
  // importer for primitives that omit their own TANGENT attribute.
  auto tangent_sum = std::vector<math::vector3f>(vertex_count, math::vector3f::zero);
  auto bitangent_sum = std::vector<math::vector3f>(vertex_count, math::vector3f::zero);

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
    auto t = tangent_sum[local] - n * math::vector3f::dot(n, tangent_sum[local]);

    t = (t.length_squared() < 1e-12f) ? math::vector3f::orthogonal(n) : math::vector3f::normalized(t);

    const auto handedness = (math::vector3f::dot(math::vector3f::cross(n, t), bitangent_sum[local]) < 0.0f) ? -1.0f : 1.0f;

    current.tangent[0] = t.x();
    current.tangent[1] = t.y();
    current.tangent[2] = t.z();
    current.tangent[3] = handedness;
  }
}

auto assets_module::_cook_mesh(const std::filesystem::path& source, const std::filesystem::path& relative_source, const math::uuid& id, const std::filesystem::path& cooked, const mesh_import_options& options) -> bool {
  auto data = fastgltf::GltfDataBuffer::FromPath(source);

  if (data.error() != fastgltf::Error::None) {
    utility::logger<"assets">::warn("Cook: could not open mesh '{}'", source.generic_string());
    return false;
  }

  auto parser = fastgltf::Parser{};

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
      return import(relative_source.parent_path() / std::filesystem::path{std::string{uri->uri.path()}});
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

    if (pbr.baseColorTexture.has_value())         description.albedo             = texture_uuid(pbr.baseColorTexture->textureIndex);
    if (pbr.metallicRoughnessTexture.has_value()) description.metallic_roughness = texture_uuid(pbr.metallicRoughnessTexture->textureIndex);
    if (gltf_material.normalTexture.has_value())  description.normal             = texture_uuid(gltf_material.normalTexture->textureIndex);
    if (gltf_material.occlusionTexture.has_value()) description.occlusion        = texture_uuid(gltf_material.occlusionTexture->textureIndex);
    if (gltf_material.emissiveTexture.has_value()) description.emissive          = texture_uuid(gltf_material.emissiveTexture->textureIndex);

    auto material_uuid = math::uuid::nil();

    if (options.extract_materials) {
      material_uuid = _extract_gltf_material(description, relative_source);
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
  auto indices = std::vector<std::uint32_t>{};
  auto submeshes = std::vector<cooked_submesh>{};
  auto mesh_volume = math::volume{};

  const auto append = [&](const fastgltf::Mesh& gltf_mesh, const fastgltf::math::fmat4x4& world) {
    for (const auto& primitive : gltf_mesh.primitives) {
      const auto* position = primitive.findAttribute("POSITION");

      if (position == primitive.attributes.end()) {
        continue;
      }

      const auto vertex_start = vertices.size();

      const auto& position_accessor = gltf.accessors[position->accessorIndex];
      vertices.resize(vertex_start + position_accessor.count);

      auto submesh_volume = math::volume{};

      fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, position_accessor, [&](fastgltf::math::fvec3 value, std::size_t index) {
        const auto world_position = world * fastgltf::math::fvec4{value[0], value[1], value[2], 1.0f};
        const auto point = math::vector3f{world_position[0], world_position[1], world_position[2]};

        auto& current = vertices[vertex_start + index];
        current.position[0] = point.x();
        current.position[1] = point.y();
        current.position[2] = point.z();

        submesh_volume.include(point);
        mesh_volume.include(point);
      });

      if (const auto* normal = primitive.findAttribute("NORMAL"); normal != primitive.attributes.end()) {
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

      if (!primitive.indicesAccessor.has_value()) {
        continue;
      }

      const auto& index_accessor = gltf.accessors[primitive.indicesAccessor.value()];
      const auto index_start = indices.size();
      indices.reserve(index_start + index_accessor.count);

      fastgltf::iterateAccessor<std::uint32_t>(gltf, index_accessor, [&](std::uint32_t index) {
        indices.push_back(static_cast<std::uint32_t>(vertex_start) + index);
      });

      if (!has_explicit_tangent) {
        _generate_tangents(vertices, indices, vertex_start, position_accessor.count, index_start, index_accessor.count);
      }

      submeshes.push_back(cooked_submesh{
        static_cast<std::uint32_t>(index_start),
        static_cast<std::uint32_t>(index_accessor.count),
        submesh_volume,
        material_uuid_for(primitive.materialIndex)
      });
    }
  };

  if (!gltf.scenes.empty()) {
    const auto scene_index = gltf.defaultScene.value_or(std::size_t{0});

    fastgltf::iterateSceneNodes(gltf, scene_index, fastgltf::math::fmat4x4{}, [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& world) {
      if (node.meshIndex.has_value()) {
        append(gltf.meshes[node.meshIndex.value()], world);
      }
    });
  } else {
    for (const auto& gltf_mesh : gltf.meshes) {
      append(gltf_mesh, fastgltf::math::fmat4x4{});
    }
  }

  if (vertices.empty() || indices.empty()) {
    utility::logger<"assets">::warn("Cook: mesh '{}' has no drawable geometry", source.generic_string());
    return false;
  }

  auto error = std::error_code{};
  std::filesystem::create_directories(cooked.parent_path(), error);

  auto out = std::ofstream{cooked, std::ios::binary};

  if (!out) {
    utility::logger<"assets">::warn("Cook: could not write '{}'", cooked.generic_string());
    return false;
  }

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

  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  out.write(reinterpret_cast<const char*>(vertices.data()), static_cast<std::streamsize>(vertices.size() * sizeof(vertex)));
  out.write(reinterpret_cast<const char*>(indices.data()), static_cast<std::streamsize>(indices.size() * sizeof(std::uint32_t)));

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

    out.write(reinterpret_cast<const char*>(&record), sizeof(record));
  }

  utility::logger<"assets">::debug("Cooked mesh '{}' -> '{}'", source.generic_string(), cooked.generic_string());

  return true;
}

auto assets_module::_load_cooked_mesh(const std::filesystem::path& cooked, std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<cooked_submesh>& submeshes, math::volume& bounds) -> bool {
  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    return false;
  }

  auto header = mesh_file_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != mesh_magic || header.version != mesh_version) {
    return false; // missing / corrupt / stale format -> caller recooks
  }

  vertices.resize(header.vertex_count);
  in.read(reinterpret_cast<char*>(vertices.data()), static_cast<std::streamsize>(header.vertex_count * sizeof(vertex)));

  indices.resize(header.index_count);
  in.read(reinterpret_cast<char*>(indices.data()), static_cast<std::streamsize>(header.index_count * sizeof(std::uint32_t)));

  submeshes.clear();
  submeshes.reserve(header.submesh_count);

  for (auto i = std::uint32_t{0u}; i < header.submesh_count; ++i) {
    auto record = submesh_file_record{};
    in.read(reinterpret_cast<char*>(&record), sizeof(record));

    if (!in) {
      return false;
    }

    submeshes.push_back(cooked_submesh{
      record.index_offset,
      record.index_count,
      math::volume{math::vector3{record.bounds_min[0], record.bounds_min[1], record.bounds_min[2]}, math::vector3{record.bounds_max[0], record.bounds_max[1], record.bounds_max[2]}},
      math::uuid::from_value(record.material_uuid)
    });
  }

  if (!in) {
    return false;
  }

  bounds = math::volume{math::vector3{header.bounds_min[0], header.bounds_min[1], header.bounds_min[2]}, math::vector3{header.bounds_max[0], header.bounds_max[1], header.bounds_max[2]}};

  return true;
}

auto assets_module::_cook_material(const math::uuid& id, const material_description& description) -> bool {
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

auto assets_module::_load_cooked_material(const std::filesystem::path& cooked, const math::uuid& id) -> material_handle {
  auto in = std::ifstream{cooked, std::ios::binary};

  if (!in) {
    utility::logger<"assets">::warn("Could not open cooked material '{}'", cooked.generic_string());
    return material_handle{};
  }

  auto header = material_file_header{};
  in.read(reinterpret_cast<char*>(&header), sizeof(header));

  if (!in || header.magic != material_magic || header.version != material_version) {
    utility::logger<"assets">::warn("Invalid cooked material '{}'", cooked.generic_string());
    return material_handle{};
  }

  auto name = std::string(header.name_length, '\0');

  if (header.name_length > 0u) {
    in.read(name.data(), static_cast<std::streamsize>(header.name_length));

    if (!in) {
      return material_handle{};
    }
  }

  auto info = material::create_info{};
  info.name = name.empty() ? std::string{"material"} : name;
  info.base_color_factor = math::color{header.base_color_factor[0], header.base_color_factor[1], header.base_color_factor[2], header.base_color_factor[3]};
  info.emissive_factor = math::vector3{header.emissive_factor[0], header.emissive_factor[1], header.emissive_factor[2]};
  info.metallic_factor = header.metallic_factor;
  info.roughness_factor = header.roughness_factor;
  info.alpha = static_cast<alpha_mode>(header.alpha_mode);
  info.alpha_cutoff = header.alpha_cutoff;
  info.is_double_sided = header.is_double_sided != 0u;

  const auto load_slot = [&](std::uint64_t uuid, graphics::format format) -> texture_handle {
    if (uuid == 0ull) {
      return texture_handle{};
    }

    return load_texture(math::uuid::from_value(uuid), format);
  };

  info.albedo = load_slot(header.albedo_uuid, graphics::format::r8g8b8a8_srgb);
  info.normal = load_slot(header.normal_uuid, graphics::format::r8g8b8a8_unorm);
  info.metallic_roughness = load_slot(header.metallic_roughness_uuid, graphics::format::r8g8b8a8_unorm);
  info.occlusion = load_slot(header.occlusion_uuid, graphics::format::r8g8b8a8_unorm);
  info.emissive = load_slot(header.emissive_uuid, graphics::format::r8g8b8a8_srgb);

  auto record = std::make_shared<material>(info);
  record->_id = id;

  auto handle = _register_material(record);

  {
    auto lock = std::lock_guard{_mutex};
    _material_files.emplace(id, record);
  }

  return handle;
}

auto assets_module::_cook_environment_map(const std::filesystem::path& source, const std::filesystem::path& cooked) -> bool {
  auto width = std::int32_t{0};
  auto height = std::int32_t{0};
  auto channels = std::int32_t{0};

  // source (from _paths[uuid]) is already fully resolved — same as _cook_texture's source.string()
  // just below in this file; wrapping it in _absolute() here would double-prefix assets_directory().
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

auto assets_module::_load_cooked_environment_map(const std::filesystem::path& cooked, std::vector<std::byte>& pixels, std::uint32_t& width, std::uint32_t& height) -> bool {
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

auto assets_module::_ensure_brdf_lut(graphics::command_buffer& command_buffer) -> void {
  if (_brdf_lut_index != environment_map::invalid_index) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  constexpr auto threads_per_group = std::uint32_t{8u};

  _brdf_lut_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{brdf_lut_size, brdf_lut_size, 1u},
    .format = graphics::format::r16g16_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .concurrent_sharing = true,
    .name = "IBL BRDF LUT"
  });

  auto& brdf_lut = registry.get<graphics::image>(_brdf_lut_image);

  auto to_general = graphics::command_buffer::image_transition_data{};
  to_general.image = brdf_lut.handle();
  to_general.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
  to_general.src_access_mask = VK_ACCESS_2_NONE;
  to_general.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  to_general.dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
  to_general.old_layout = graphics::image_layout::undefined;
  to_general.new_layout = graphics::image_layout::general;
  to_general.aspect_mask = brdf_lut.aspect();
  command_buffer.transition_image_layout(to_general);

  // Leaked forever, on purpose: this runs exactly once per program lifetime, so recovering this
  // one bindless storage-image slot after the bake isn't worth the added bookkeeping — see
  // _bake_environment, which does defer cleanup, since that one runs on every environment load.
  const auto output_index = bindless_table.register_storage_image(brdf_lut.view());

  bindless_table.flush_writes();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto shader = shader_cache.get({"shaders/pbr/brdf_lut.slang", entry_points});

  auto pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = shader,
    .name = "IBL BRDF LUT"
  });

  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  command_buffer.bind_pipeline(*pipeline);

  struct push_data {
    std::uint32_t output_index;
  }; // struct push_data

  auto push = push_data{output_index};

  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &push, sizeof(push));
  command_buffer.push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  const auto groups = (brdf_lut_size + threads_per_group - 1u) / threads_per_group;
  command_buffer.dispatch(groups, groups, 1u);

  auto to_read = graphics::command_buffer::image_transition_data{};
  to_read.image = brdf_lut.handle();
  to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  to_read.src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
  to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
  to_read.old_layout = graphics::image_layout::general;
  to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
  to_read.aspect_mask = brdf_lut.aspect();
  command_buffer.transition_image_layout(to_read);

  _brdf_lut_index = bindless_table.reserve_sampled_image();
  bindless_table.write_sampled_image(_brdf_lut_index, brdf_lut.view());
  bindless_table.flush_writes();
}

auto assets_module::_bake_environment(environment_map& record, const std::vector<std::byte>& pixels, std::uint32_t width, std::uint32_t height) -> void {
  constexpr auto threads_per_group = std::uint32_t{8u};

  // Bindless indices that must stay untouched until this whole command buffer has actually
  // finished on the GPU: vkUpdateDescriptorSets takes effect immediately on the host, not at
  // GPU-execution time, so reusing one of these slots for something else before submit_idle()
  // returns could make an *earlier* dispatch in this same buffer see the *later* write once the
  // GPU finally gets around to executing it.
  struct transient_storage {
    std::uint32_t index;
    VkImageView view;
  }; // struct transient_storage

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  const auto sampler_index = bindless_table.sampler_index(graphics::sampler::create_info{});

  auto command_buffer = graphics::command_buffer{graphics::queue::type::compute, true};

  auto pending_storage = std::vector<transient_storage>{};
  auto pending_sampled = std::vector<std::uint32_t>{};

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  // --- Radiance: upload the equirectangular source. Persistent — the skybox pass samples this
  // same index directly, so it isn't scratch data like the cube derived from it below. ---

  const auto radiance_handle = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{width, height, 1u},
    .format = graphics::format::r32g32b32a32_sfloat,
    .usage = graphics::image_usage::transfer_destination | graphics::image_usage::sampled,
    .concurrent_sharing = true,
    .name = "Environment Radiance"
  });

  auto& radiance = registry.get<graphics::image>(radiance_handle);

  auto staging = graphics::buffer{graphics::buffer::create_info{
    .size = static_cast<graphics::buffer::size_type>(pixels.size()),
    .usage = graphics::buffer_usage::transfer_source,
    .memory = graphics::memory_usage::host_write,
    .name = "Environment Staging"
  }};

  staging.write(pixels.data(), static_cast<graphics::buffer::size_type>(pixels.size()));

  {
    auto to_transfer = graphics::command_buffer::image_transition_data{};
    to_transfer.image = radiance.handle();
    to_transfer.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    to_transfer.src_access_mask = VK_ACCESS_2_NONE;
    to_transfer.dst_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_transfer.dst_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_transfer.old_layout = graphics::image_layout::undefined;
    to_transfer.new_layout = graphics::image_layout::transfer_destination_optimal;
    to_transfer.aspect_mask = radiance.aspect();
    command_buffer.transition_image_layout(to_transfer);

    auto region = VkBufferImageCopy{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1u;
    region.imageExtent = VkExtent3D{width, height, 1u};
    vkCmdCopyBufferToImage(command_buffer.handle(), staging.handle(), radiance.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);

    auto to_read = graphics::command_buffer::image_transition_data{};
    to_read.image = radiance.handle();
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_read.src_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = graphics::image_layout::transfer_destination_optimal;
    to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
    to_read.aspect_mask = radiance.aspect();
    command_buffer.transition_image_layout(to_read);
  }

  const auto radiance_index = bindless_table.register_sampled_image(radiance.view());

  // --- Equirect -> cubemap: a transient, single-mip cube used only as convolution input below;
  // never touched by the graphics queue, so plain exclusive sharing (the image default) is fine.
  // Single mip, not a chain: both irradiance.slang and prefilter.slang always sample this at mip
  // 0 (see prefilter.slang's doc comment on why it stopped doing solid-angle-driven mip
  // selection), so building a mip chain here would just be wasted bake-time work. ---

  auto radiance_cube = graphics::image{graphics::image::create_info{
    .extent = math::vector3u{radiance_cube_size, radiance_cube_size, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .array_layers = 6u,
    .view_type = graphics::image_view_type::cube,
    .name = "Environment Radiance Cube (scratch)"
  }};

  const auto equirect_to_cube_shader = shader_cache.get({"shaders/pbr/equirect_to_cubemap.slang", entry_points});
  auto equirect_to_cube_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{.shader = equirect_to_cube_shader, .name = "IBL Equirect To Cubemap"});

  {
    auto to_general = graphics::command_buffer::image_transition_data{};
    to_general.image = radiance_cube.handle();
    to_general.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    to_general.src_access_mask = VK_ACCESS_2_NONE;
    to_general.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_general.dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_general.old_layout = graphics::image_layout::undefined;
    to_general.new_layout = graphics::image_layout::general;
    to_general.aspect_mask = radiance_cube.aspect();
    to_general.layer_count = 6u;
    command_buffer.transition_image_layout(to_general);

    const auto mip0_view = radiance_cube.create_view(graphics::image_view_type::two_dimensional_array, 0u, 1u, 0u, 6u);
    const auto output_index = bindless_table.register_storage_cube(mip0_view);
    pending_storage.push_back({output_index, mip0_view});

    bindless_table.flush_writes();

    struct push_data {
      std::uint32_t source_index;
      std::uint32_t sampler_index;
      std::uint32_t output_index;
    }; // struct push_data

    auto push = push_data{radiance_index, sampler_index, output_index};

    auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
    std::memcpy(range.data(), &push, sizeof(push));

    command_buffer.bind_pipeline(*equirect_to_cube_pipeline);
    command_buffer.push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

    const auto groups = (radiance_cube_size + threads_per_group - 1u) / threads_per_group;
    command_buffer.dispatch(groups, groups, 6u);
  }

  {
    auto to_read = graphics::command_buffer::image_transition_data{};
    to_read.image = radiance_cube.handle();
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = graphics::image_layout::general;
    to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
    to_read.aspect_mask = radiance_cube.aspect();
    to_read.layer_count = 6u;
    command_buffer.transition_image_layout(to_read);
  }

  const auto radiance_cube_sampled_index = bindless_table.register_sampled_cube(radiance_cube.view());
  pending_sampled.push_back(radiance_cube_sampled_index);

  bindless_table.flush_writes();

  // --- Irradiance: single-mip cube, cosine-weighted hemisphere convolution ---

  const auto irradiance_handle = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{irradiance_cube_size, irradiance_cube_size, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .array_layers = 6u,
    .view_type = graphics::image_view_type::cube,
    .concurrent_sharing = true,
    .name = "IBL Irradiance"
  });

  auto& irradiance = registry.get<graphics::image>(irradiance_handle);

  {
    auto to_general = graphics::command_buffer::image_transition_data{};
    to_general.image = irradiance.handle();
    to_general.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    to_general.src_access_mask = VK_ACCESS_2_NONE;
    to_general.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_general.dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_general.old_layout = graphics::image_layout::undefined;
    to_general.new_layout = graphics::image_layout::general;
    to_general.aspect_mask = irradiance.aspect();
    to_general.layer_count = 6u;
    command_buffer.transition_image_layout(to_general);

    const auto view = irradiance.create_view(graphics::image_view_type::two_dimensional_array, 0u, 1u, 0u, 6u);
    const auto output_index = bindless_table.register_storage_cube(view);
    pending_storage.push_back({output_index, view});

    bindless_table.flush_writes();

    const auto shader = shader_cache.get({"shaders/pbr/irradiance.slang", entry_points});
    auto pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{.shader = shader, .name = "IBL Irradiance"});

    struct push_data {
      std::uint32_t source_index;
      std::uint32_t sampler_index;
      std::uint32_t output_index;
    }; // struct push_data

    auto push = push_data{radiance_cube_sampled_index, sampler_index, output_index};

    auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
    std::memcpy(range.data(), &push, sizeof(push));

    command_buffer.bind_pipeline(*pipeline);
    command_buffer.push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

    const auto groups = (irradiance_cube_size + threads_per_group - 1u) / threads_per_group;
    command_buffer.dispatch(groups, groups, 6u);
  }

  const auto irradiance_index = bindless_table.register_sampled_cube(irradiance.view());

  // --- Prefiltered specular: a real mip chain now, one dispatch per mip with a roughness push
  // constant, rather than N unrelated discrete images blended in the shader. ---

  const auto prefiltered_handle = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{prefiltered_cube_size, prefiltered_cube_size, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .mip_levels = prefiltered_mip_count,
    .array_layers = 6u,
    .view_type = graphics::image_view_type::cube,
    .concurrent_sharing = true,
    .name = "IBL Prefiltered"
  });

  auto& prefiltered = registry.get<graphics::image>(prefiltered_handle);

  {
    auto to_general = graphics::command_buffer::image_transition_data{};
    to_general.image = prefiltered.handle();
    to_general.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    to_general.src_access_mask = VK_ACCESS_2_NONE;
    to_general.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_general.dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_general.old_layout = graphics::image_layout::undefined;
    to_general.new_layout = graphics::image_layout::general;
    to_general.aspect_mask = prefiltered.aspect();
    to_general.mip_levels = prefiltered_mip_count;
    to_general.layer_count = 6u;
    command_buffer.transition_image_layout(to_general);

    const auto shader = shader_cache.get({"shaders/pbr/prefilter.slang", entry_points});
    auto pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{.shader = shader, .name = "IBL Prefilter"});

    command_buffer.bind_pipeline(*pipeline);

    struct push_data {
      std::uint32_t source_index;
      std::uint32_t sampler_index;
      std::uint32_t output_index;
      std::float_t roughness;
    }; // struct push_data

    for (auto mip = std::uint32_t{0u}; mip < prefiltered_mip_count; ++mip) {
      const auto mip_size = std::max(prefiltered_cube_size >> mip, 1u);

      const auto view = prefiltered.create_view(graphics::image_view_type::two_dimensional_array, mip, 1u, 0u, 6u);
      const auto output_index = bindless_table.register_storage_cube(view);
      pending_storage.push_back({output_index, view});

      bindless_table.flush_writes();

      const auto roughness = static_cast<std::float_t>(mip) / static_cast<std::float_t>(prefiltered_mip_count - 1u);

      auto push = push_data{radiance_cube_sampled_index, sampler_index, output_index, roughness};

      auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
      std::memcpy(range.data(), &push, sizeof(push));

      command_buffer.push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

      const auto groups = (mip_size + threads_per_group - 1u) / threads_per_group;
      command_buffer.dispatch(groups, groups, 6u);
    }
  }

  const auto prefiltered_index = bindless_table.register_sampled_cube(prefiltered.view());

  // --- BRDF LUT: environment-independent, baked once and shared by everything ---

  _ensure_brdf_lut(command_buffer);

  // --- Every persistent output goes shader-readable, then one blocking submit ---

  {
    auto to_read = graphics::command_buffer::image_transition_data{};
    to_read.image = irradiance.handle();
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = graphics::image_layout::general;
    to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
    to_read.aspect_mask = irradiance.aspect();
    to_read.layer_count = 6u;
    command_buffer.transition_image_layout(to_read);
  }

  {
    auto to_read = graphics::command_buffer::image_transition_data{};
    to_read.image = prefiltered.handle();
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = graphics::image_layout::general;
    to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
    to_read.aspect_mask = prefiltered.aspect();
    to_read.mip_levels = prefiltered_mip_count;
    to_read.layer_count = 6u;
    command_buffer.transition_image_layout(to_read);
  }

  command_buffer.submit_idle();

  // Only now that the GPU has actually finished is it safe to recycle these slots (see the
  // comment on transient_storage above) and destroy their transient views.
  for (const auto& entry : pending_storage) {
    bindless_table.unregister_storage_cube(entry.index);
    vkDestroyImageView(graphics_module.logical_device(), entry.view, nullptr);
  }

  for (const auto index : pending_sampled) {
    bindless_table.unregister_sampled_cube(index);
  }

  record._radiance_index = radiance_index;
  record._irradiance_index = irradiance_index;
  record._prefiltered_index = prefiltered_index;
  record._prefiltered_mip_count = prefiltered_mip_count;

  utility::logger<"assets">::info("Baked IBL for environment {}", record.id());
}

auto assets_module::_derive_material_uuid(const math::uuid& mesh, std::size_t index) -> math::uuid {
  // splitmix64 over (mesh uuid, index) — deterministic so re-cooking is stable.
  auto x = mesh.value() ^ (0x9e3779b97f4a7c15ull * (static_cast<std::uint64_t>(index) + 1ull));
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ull;
  x ^= x >> 27; x *= 0x94d049bb133111ebull;
  x ^= x >> 31;

  return math::uuid::from_value(x == 0ull ? 1ull : x); // never nil
}

} // namespace sbx::assets
