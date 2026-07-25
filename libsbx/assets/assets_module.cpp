// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/assets_module.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <span>
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

#include <libsbx/memory/alignment.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/vector3.hpp>

namespace sbx::assets {

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
  std::uint32_t padding;
}; // struct material_data

assets_module::assets_module() {
  // Reserve the neutral fallbacks first so their bindless indices are stable. Uploaded by the first
  // process_uploads, resident from frame 1 — so materials can point any absent slot at one of these.
  _white = _create_default_texture({255u, 255u, 255u, 255u});
  _normal = _create_default_texture({128u, 128u, 255u, 255u}); // (0,0,1) tangent-space normal
  _black = _create_default_texture({0u, 0u, 0u, 255u});
  _magenta = _create_default_texture({255u, 0u, 255u, 255u});   // load-error marker
}

assets_module::~assets_module() { }

auto assets_module::import(const std::filesystem::path& path) -> math::uuid {
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
  }

  return uuid;
}

auto assets_module::import_directory(const std::filesystem::path& root) -> void {
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

    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".gltf" || extension == ".glb" || extension == ".material") {
      import(entry.path());
    }
  }
}

auto assets_module::load_texture(const math::uuid& id, graphics::format format) -> texture_handle {
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

  auto width = std::int32_t{0};
  auto height = std::int32_t{0};
  auto channels = std::int32_t{0};

  auto* data = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (data == nullptr) {
    utility::logger<"assets">::warn("Could not load texture '{}'", path.generic_string());
  
    return texture_handle{};
  }

  const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;

  auto pixels = std::vector<std::byte>{reinterpret_cast<const std::byte*>(data), reinterpret_cast<const std::byte*>(data) + count};

  stbi_image_free(data);

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& bindless_table = graphics_module.bindless_table();

  const auto index = bindless_table.reserve_sampled_image();

  auto record = std::make_shared<texture>(texture{index});
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};

    _textures.emplace(key, record);
    _pending_textures.push_back(pending_texture_upload{index, std::move(pixels), static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), format});
  }

  return texture_handle{record};
}

auto assets_module::load_texture(const std::filesystem::path& path, graphics::format format) -> texture_handle {
  return load_texture(import(path), format);
}

auto assets_module::load_mesh(const math::uuid& id) -> mesh_handle {
  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _meshes.find(id); entry != _meshes.end()) {
      return mesh_handle{entry->second};
    }
  }

  auto path = std::filesystem::path{};

  {
    auto lock = std::lock_guard{_mutex};
    const auto entry = _paths.find(id);
    if (entry == _paths.end()) {
      utility::logger<"assets">::warn("Unknown mesh uuid {}", id);
      return mesh_handle{};
    }
    path = entry->second;
  }

  auto data = fastgltf::GltfDataBuffer::FromPath(path);

  if (data.error() != fastgltf::Error::None) {
    utility::logger<"assets">::warn("Could not open mesh '{}'", path.generic_string());
  
    return mesh_handle{};
  }

  auto parser = fastgltf::Parser{};

  auto loaded = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices);

  if (loaded.error() != fastgltf::Error::None) {
    utility::logger<"assets">::warn("Could not parse mesh '{}'", path.generic_string());

    return mesh_handle{};
  }

  auto& gltf = loaded.get();

  auto vertices = std::vector<vertex>{};
  auto indices = std::vector<std::uint32_t>{};
  auto submeshes = std::vector<mesh::submesh>{};
  auto materials = utility::make_reserved_vector<material_handle>(gltf.materials.size());

  auto mesh_volume = math::volume{};

  const auto load_gltf_texture = [&](std::size_t texture_index, graphics::format format) -> texture_handle {
    const auto& gltf_texture = gltf.textures[texture_index];

    if (!gltf_texture.imageIndex.has_value()) { 
      return texture_handle{}; 
    }

    const auto& image = gltf.images[gltf_texture.imageIndex.value()];

    if (const auto* uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
      return load_texture(path.parent_path() / std::filesystem::path{std::string{uri->uri.path()}}, format);
    }

    utility::logger<"assets">::warn("Mesh '{}': non-file image, using default", path.generic_string());

    return texture_handle{};
  };

  for (const auto& gltf_material : gltf.materials) {
    auto info = material::create_info{};

    const auto& pbr = gltf_material.pbrData;

    info.name = gltf_material.name.empty() ? std::string{"material"} : std::string{gltf_material.name.begin(), gltf_material.name.end()};
    info.base_color_factor = math::color{pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]};
    info.metallic_factor = pbr.metallicFactor;
    info.roughness_factor = pbr.roughnessFactor;
    info.emissive_factor = math::vector3{gltf_material.emissiveFactor[0], gltf_material.emissiveFactor[1], gltf_material.emissiveFactor[2]};

    if (pbr.baseColorTexture.has_value()) {
      info.albedo = load_gltf_texture(pbr.baseColorTexture->textureIndex, graphics::format::r8g8b8a8_srgb);
    }

    if (pbr.metallicRoughnessTexture.has_value()) {
      info.metallic_roughness = load_gltf_texture(pbr.metallicRoughnessTexture->textureIndex, graphics::format::r8g8b8a8_unorm);
    }

    if (gltf_material.normalTexture.has_value()) {
      info.normal = load_gltf_texture(gltf_material.normalTexture->textureIndex, graphics::format::r8g8b8a8_unorm);
    }

    if (gltf_material.occlusionTexture.has_value()) {
      info.occlusion = load_gltf_texture(gltf_material.occlusionTexture->textureIndex, graphics::format::r8g8b8a8_unorm);
    }

    if (gltf_material.emissiveTexture.has_value()) {
      info.emissive = load_gltf_texture(gltf_material.emissiveTexture->textureIndex, graphics::format::r8g8b8a8_srgb);
    }

    materials.push_back(create_material(info));
  }

  auto fallback_material = material_handle{};

  const auto material_for = [&](fastgltf::Optional<std::size_t> index) -> material_handle {
    if (index.has_value() && index.value() < materials.size()) {
      return materials[index.value()];
    }

    if (!fallback_material.is_valid()) {
      fallback_material = create_material(material::create_info{
        .albedo = _magenta
      });
    }

    return fallback_material;
  };

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

      if (const auto* tangent = primitive.findAttribute("TANGENT"); tangent != primitive.attributes.end()) {
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

      submeshes.push_back(mesh::submesh{
        static_cast<std::uint32_t>(index_start),
        static_cast<std::uint32_t>(index_accessor.count),
        submesh_volume,
        material_for(primitive.materialIndex)
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
    utility::logger<"assets">::warn("Mesh '{}' has no drawable geometry", path.generic_string());

    return mesh_handle{};
  }

  const auto vertex_count = vertices.size();
  const auto index_count = indices.size();
  const auto submesh_count = submeshes.size();

  auto record = std::make_shared<mesh>(std::move(submeshes), mesh_volume);
  record->_id = id;

  {
    auto lock = std::lock_guard{_mutex};

    _meshes.emplace(id, record);
    _pending_meshes.push_back(pending_mesh_upload{record, std::move(vertices), std::move(indices)});
  }

  utility::logger<"assets">::info("Loaded mesh '{}': {} vertices, {} indices, {} submeshes", path.generic_string(), vertex_count, index_count, submesh_count);

  return mesh_handle{record};
}

auto assets_module::load_mesh(const std::filesystem::path& path) -> mesh_handle {
  return load_mesh(import(path));
}

auto assets_module::load_material(const math::uuid& id) -> material_handle {
  {
    auto lock = std::lock_guard{_mutex};
    if (const auto entry = _material_files.find(id); entry != _material_files.end()) {
      return material_handle{entry->second};
    }
  }

  auto path = std::filesystem::path{};
  {
    auto lock = std::lock_guard{_mutex};
    const auto entry = _paths.find(id);
    if (entry == _paths.end()) {
      utility::logger<"assets">::warn("Unknown material uuid {}", id);
      return material_handle{};
    }
    path = entry->second;
  }

  auto root = YAML::Node{};
  try {
    root = YAML::LoadFile(path.string());
  } catch (const std::exception& exception) {
    utility::logger<"assets">::warn("Could not parse material '{}' ({})", path.generic_string(), exception.what());
    return material_handle{};
  }

  auto info = material::create_info{};

  if (root["name"]) { 
    info.name = root["name"].as<std::string>(); 
  }

  if (root["base_color_factor"]) { 
    info.base_color_factor = root["base_color_factor"].as<math::color>(); 
  }

  if (root["emissive_factor"]) { 
    info.emissive_factor = root["emissive_factor"].as<math::vector3>(); 
  }

  if (root["metallic_factor"]) { 
    info.metallic_factor = root["metallic_factor"].as<std::float_t>(); 
  }

  if (root["roughness_factor"])  { 
    info.roughness_factor = root["roughness_factor"].as<std::float_t>(); 
  }

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

auto assets_module::load_material(const std::filesystem::path& path) -> material_handle {
  return load_material(import(path));
}

auto assets_module::create_material(const material::create_info& create_info) -> material_handle {
  return _register_material(std::make_shared<material>(create_info));
}

auto assets_module::save_material(const material_handle& material, const std::filesystem::path& path) -> void {
  if (!material.is_valid()) {
    utility::logger<"assets">::warn("Cannot save an invalid material to '{}'", path.generic_string());
    return;
  }

  const auto path_of = [this](const texture_handle& texture) -> std::optional<std::string> {
    if (!texture.is_valid()) {
      return std::nullopt;
    }

    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _paths.find(texture->id()); entry != _paths.end()) {
      return entry->second.generic_string();
    }

    return std::nullopt; // default/procedural texture (nil uuid) — omit the slot
  };

  auto node = YAML::Node{};

  node["name"] = material->name();
  node["base_color_factor"] = material->base_color_factor();
  node["emissive_factor"] = material->emissive_factor();
  node["metallic_factor"] = material->metallic_factor();
  node["roughness_factor"] = material->roughness_factor();

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

  if (!path.parent_path().empty()) {
    std::filesystem::create_directories(path.parent_path());
  }

  auto out = std::ofstream{path};
  out << node;

  import(path); // register + create the .meta so it's a first-class asset

  utility::logger<"assets">::info("Saved material '{}'", path.generic_string());
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
    const auto handle = registry.emplace<graphics::image>(graphics::image::create_info{
      .extent = math::vector3u{request.width, request.height, 1u},
      .format = request.format,
      .usage = graphics::image_usage::transfer_destination | graphics::image_usage::sampled,
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

} // namespace sbx::assets
