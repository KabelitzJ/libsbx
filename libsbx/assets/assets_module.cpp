// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/assets_module.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/math.hpp>

#include <libsbx/memory/alignment.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/vector3.hpp>

namespace sbx::assets {

struct material_data {
  math::vector4 base_color_factor;
  std::uint32_t albedo_index;
  std::uint32_t padding[3];
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

auto assets_module::load_texture(const std::filesystem::path& path) -> texture_handle {
  const auto key = path.generic_string();

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _textures.find(key); entry != _textures.end()) {
      return texture_handle{entry->second};
    }
  }

  auto width = std::int32_t{0};
  auto height = std::int32_t{0};
  auto channels = std::int32_t{0};

  auto* data = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (data == nullptr) {
    utility::logger<"assets">::warn("Could not load texture '{}'", key);

    return texture_handle{};
  }

  const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;

  auto pixels = std::vector<std::byte>{reinterpret_cast<const std::byte*>(data), reinterpret_cast<const std::byte*>(data) + count};

  stbi_image_free(data);

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& bindless_table = graphics_module.bindless_table();

  const auto index = bindless_table.reserve_sampled_image();

  auto record = std::make_shared<texture>(texture{index});

  {
    auto lock = std::lock_guard{_mutex};

    _textures.emplace(key, record);
    _pending_textures.push_back(pending_texture_upload{index, std::move(pixels), static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), graphics::format::r8g8b8a8_unorm});
  }

  return texture_handle{record};
}

auto assets_module::load_mesh(const std::filesystem::path& path) -> mesh_handle {
  const auto key = path.generic_string();

  {
    auto lock = std::lock_guard{_mutex};

    if (const auto entry = _meshes.find(key); entry != _meshes.end()) {
      return mesh_handle{entry->second};
    }
  }

  auto data = fastgltf::GltfDataBuffer::FromPath(path);

  if (data.error() != fastgltf::Error::None) {
    utility::logger<"assets">::warn("Could not open mesh '{}'", key);

    return mesh_handle{};
  }

  auto parser = fastgltf::Parser{};

  auto loaded = parser.loadGltf(data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices);

  if (loaded.error() != fastgltf::Error::None) {
    utility::logger<"assets">::warn("Could not parse mesh '{}'", key);

    return mesh_handle{};
  }

  auto& gltf = loaded.get();

  auto vertices = std::vector<vertex>{};
  auto indices = std::vector<std::uint32_t>{};
  auto submeshes = std::vector<mesh::submesh>{};

  auto mesh_min = math::vector3f{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
  auto mesh_max = math::vector3f{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};

  const auto append = [&](const fastgltf::Mesh& gltf_mesh, const fastgltf::math::fmat4x4& world) {
    for (const auto& primitive : gltf_mesh.primitives) {
      const auto* position = primitive.findAttribute("POSITION");

      if (position == primitive.attributes.end()) {
        continue;
      }

      const auto vertex_start = vertices.size();

      const auto& position_accessor = gltf.accessors[position->accessorIndex];
      vertices.resize(vertex_start + position_accessor.count);

      auto submesh_min = math::vector3f{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
      auto submesh_max = math::vector3f{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};

      fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, position_accessor, [&](fastgltf::math::fvec3 value, std::size_t index) {
        const auto world_position = world * fastgltf::math::fvec4{value[0], value[1], value[2], 1.0f};
        const auto point = math::vector3f{world_position[0], world_position[1], world_position[2]};

        auto& current = vertices[vertex_start + index];
        current.position[0] = point.x();
        current.position[1] = point.y();
        current.position[2] = point.z();

        submesh_min = math::vector3f::min(submesh_min, point);
        submesh_max = math::vector3f::max(submesh_max, point);
        mesh_min = math::vector3f::min(mesh_min, point);
        mesh_max = math::vector3f::max(mesh_max, point);
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

      submeshes.push_back(mesh::submesh{static_cast<std::uint32_t>(index_start), static_cast<std::uint32_t>(index_accessor.count), math::volume{submesh_min, submesh_max}});
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
    utility::logger<"assets">::warn("Mesh '{}' has no drawable geometry", key);
    return mesh_handle{};
  }

  const auto vertex_count = vertices.size();
  const auto index_count = indices.size();
  const auto submesh_count = submeshes.size();

  auto record = std::make_shared<mesh>(std::move(submeshes), math::volume{mesh_min, mesh_max});

  {
    auto lock = std::lock_guard{_mutex};

    _meshes.emplace(key, record);
    _pending_meshes.push_back(pending_mesh_upload{record, std::move(vertices), std::move(indices)});
  }

  utility::logger<"assets">::info("Loaded mesh '{}': {} vertices, {} indices, {} submeshes", key, vertex_count, index_count, submesh_count);

  return mesh_handle{record};
}

auto assets_module::create_material(const material::create_info& create_info) -> material_handle {
  auto record = std::make_shared<material>(create_info);

  auto lock = std::lock_guard{_mutex};

  utility::assert_that(_material_count < material_capacity, "Exceeded material capacity");

  record->_index = _material_count++;

  _materials.push_back(record);
  _pending_materials.push_back(pending_material_upload{record});

  return material_handle{record};
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

    auto lock = std::lock_guard{_mutex};

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

    // Absent albedo -> white default (always resident).
    const auto albedo = material.albedo().is_valid() ? material.albedo()->index() : _white->index();

    auto data = material_data{};
    data.base_color_factor = math::vector4{material.base_color_factor().r(), material.base_color_factor().g(), material.base_color_factor().b(), material.base_color_factor().a()};
    data.albedo_index = albedo;

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

} // namespace sbx::assets
