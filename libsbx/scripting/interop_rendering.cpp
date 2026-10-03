// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/interop.hpp>

#include <cstring>
#include <filesystem>
#include <optional>
#include <vector>

#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/vector4.hpp>

#include <libsbx/assets/asset_cooker.hpp>
#include <libsbx/assets/assets_module.hpp>

#include <vulkan/vulkan.h>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/scripting/scripting_module.hpp>

namespace sbx::scripting {

auto build_script_mesh(math::vector3* positions, math::vector3* normals, math::vector2* uvs, math::color* colors, std::uint32_t vertex_count, std::uint32_t* indices, std::uint32_t index_count) -> pending_geometry {
  auto geometry = pending_geometry{};
  geometry.vertices.reserve(vertex_count);

  for (auto index = std::uint32_t{0}; index < vertex_count; ++index) {
    auto vertex = assets::vertex{positions[index], normals[index], uvs[index], math::vector4{1.0f, 0.0f, 0.0f, 1.0f}};

    if (colors) {
      vertex.color = colors[index];
    }

    geometry.vertices.push_back(vertex);
    geometry.bounds.include(positions[index]);
  }

  geometry.indices = std::vector<std::uint32_t>{indices, indices + index_count};
  assets::asset_cooker::generate_tangents(geometry.vertices, geometry.indices, 0u, vertex_count, 0u, index_count);

  return geometry;
}

auto interop::apply_pending_geometry() -> void {
  // Creating and retiring instance buffers both write the resource registry, so only while the render thread is idle.
  if (auto& instances = core::engine::get_module<scripting_module>().resources().pending_instances; !instances.empty()) {
    auto& scene = core::engine::get_module<scenes::scenes_module>().active_scene();

    for (auto& [uuid, data] : instances) {
      auto node = scene.find(math::uuid::from_value(uuid));

      if (auto renderer = node.is_valid() ? node.try_get_component<scenes::instanced_mesh_renderer>() : nullptr) {
        renderer->instances = data.empty() ? nullptr : std::make_shared<const scenes::instance_buffer>(data);
      }
    }

    instances.clear();
  }

  core::engine::get_module<scenes::scenes_module>().collect_released_instance_buffers();

  auto& pending = core::engine::get_module<scripting_module>().resources().pending_geometries;

  if (pending.empty()) {
    return;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto& scene = scenes_module.active_scene();

  for (auto& [uuid, geometry] : pending) {
    auto node = scene.find(math::uuid::from_value(uuid));

    if (!node.is_valid()) {
      continue;
    }

    if (geometry.instanced) {
      if (auto instanced = node.try_get_component<scenes::instanced_mesh_renderer>()) {
        assets_module.release_mesh(instanced->mesh);
        instanced->mesh = assets_module.create_dynamic_mesh(geometry.vertices, geometry.indices, std::move(geometry.submeshes), geometry.bounds);
      }

      continue;
    }

    auto renderer = node.try_get_component<scenes::mesh_renderer>();

    if (!renderer) {
      continue;
    }

    // Meshes are never reused in place, so the replaced one must be released or its buffers leak.
    assets_module.release_mesh(renderer->mesh);

    // create_dynamic_mesh is resident immediately, so there is no frame without a mesh.
    renderer->mesh = assets_module.create_dynamic_mesh(geometry.vertices, geometry.indices, std::move(geometry.submeshes), geometry.bounds);
  }

  pending.clear();
}

auto interop::mesh_renderer_set_geometry(std::uint64_t uuid, math::vector3* positions, math::vector3* normals, math::vector2* uvs, math::color* colors, std::uint32_t vertex_count, std::uint32_t* indices, std::uint32_t index_count, math::color* tint) -> void {
  if (!positions || !normals || !uvs || !indices || vertex_count == 0u || index_count == 0u) {
    utility::logger<"scripting">::error("Attempting to call mesh_renderer_set_geometry with invalid geometry");

    return;
  }

  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call mesh_renderer_set_geometry on an invalid node");

    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto geometry = build_script_mesh(positions, normals, uvs, colors, vertex_count, indices, index_count);

  auto& renderer = node.get_or_add_component<scenes::mesh_renderer>();

  // Reused across calls: create_material has a fixed capacity that per-frame calls would exhaust.
  auto material = assets::material_handle{};

  if (!renderer.materials.empty() && renderer.materials.front().is_valid()) {
    material = renderer.materials.front();

    if (tint) {
      assets_module.update_material(material, assets::material::create_info{
        .base_color_factor = *tint,
        .metallic_factor = 0.0f,
        .roughness_factor = 0.8f
      });
    }
  } else {
    material = assets_module.create_material(assets::material::create_info{
      .name = "Script Mesh",
      .base_color_factor = tint ? *tint : math::color::white(),
      .metallic_factor = 0.0f,
      .roughness_factor = 0.8f
    });
  }

  geometry.submeshes = std::vector<assets::mesh::submesh>{assets::mesh::submesh{0u, index_count, geometry.bounds, material}};

  // Swapped in by apply_pending_geometry once the render thread is done with the current mesh.
  core::engine::get_module<scripting_module>().resources().pending_geometries[uuid] = std::move(geometry);

  renderer.materials = std::vector<assets::material_handle>{material};
}

auto interop::instanced_mesh_renderer_set_geometry(std::uint64_t uuid, math::vector3* positions, math::vector3* normals, math::vector2* uvs, math::color* colors, std::uint32_t vertex_count, std::uint32_t* indices, std::uint32_t index_count) -> void {
  if (!positions || !normals || !uvs || !indices || vertex_count == 0u || index_count == 0u) {
    utility::logger<"scripting">::error("instanced_mesh_renderer_set_geometry: invalid geometry");
    return;
  }

  auto node = core::engine::get_module<scenes::scenes_module>().active_scene().find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("instanced_mesh_renderer_set_geometry: invalid node");
    return;
  }

  auto& renderer = node.get_or_add_component<scenes::instanced_mesh_renderer>();

  if (!renderer.material.is_valid()) {
    renderer.material = core::engine::get_module<assets::assets_module>().create_material(assets::material::create_info{
      .name = "Script Instanced Mesh",
      .metallic_factor = 0.0f,
      .roughness_factor = 0.8f
    });
  }

  auto geometry = build_script_mesh(positions, normals, uvs, colors, vertex_count, indices, index_count);
  geometry.submeshes = std::vector<assets::mesh::submesh>{assets::mesh::submesh{0u, index_count, geometry.bounds, renderer.material}};
  geometry.instanced = true;
  core::engine::get_module<scripting_module>().resources().pending_geometries[uuid] = std::move(geometry);
}

auto interop::instanced_mesh_renderer_set_material(std::uint64_t uuid, std::uint64_t material_uuid) -> void {
  auto node = core::engine::get_module<scenes::scenes_module>().active_scene().find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("instanced_mesh_renderer_set_material: invalid node");
    return;
  }

  node.get_or_add_component<scenes::instanced_mesh_renderer>().material = core::engine::get_module<assets::assets_module>().load_material(math::uuid::from_value(material_uuid));
}

auto interop::instanced_mesh_renderer_set_instances(std::uint64_t uuid, scenes::instance_data* instances, std::uint32_t count) -> void {
  auto node = core::engine::get_module<scenes::scenes_module>().active_scene().find(math::uuid::from_value(uuid));

  if (!node.is_valid() || (!instances && count > 0u)) {
    utility::logger<"scripting">::error("instanced_mesh_renderer_set_instances: invalid node or data");
    return;
  }

  node.get_or_add_component<scenes::instanced_mesh_renderer>();
  core::engine::get_module<scripting_module>().resources().pending_instances[uuid] = std::vector<scenes::instance_data>{instances, instances + count};
}

auto interop::mesh_renderer_set_material(std::uint64_t uuid, std::uint32_t submesh_index, std::uint64_t material_uuid) -> void {
  auto& scenes_module = core::engine::get_module<scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();
  auto node = scene.find(math::uuid::from_value(uuid));

  if (!node.is_valid()) {
    utility::logger<"scripting">::error("Attempting to call mesh_renderer_set_material on an invalid node");

    return;
  }

  auto& renderer = node.get_or_add_component<scenes::mesh_renderer>();

  if (renderer.materials.size() <= submesh_index) {
    renderer.materials.resize(submesh_index + 1u);
  }

  if (material_uuid == 0u) {
    renderer.materials[submesh_index] = assets::material_handle{};

    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  renderer.materials[submesh_index] = assets_module.load_material(math::uuid::from_value(material_uuid));
}

auto interop::material_load(managed::string path) -> std::uint64_t {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto material = assets_module.load_material(std::filesystem::path{std::string{path}});

  return material.is_valid() ? material->id().value() : 0u;
}

auto interop::texture_load(managed::string path, std::uint32_t format) -> std::uint64_t {
  const auto native_format = [format]() -> graphics::format {
    switch (format) {
      case 0u: return graphics::format::r8g8b8a8_unorm;
      case 1u: return graphics::format::r32_sfloat;
      case 2u: return graphics::format::r8_unorm;
      case 3u: return graphics::format::r8g8b8a8_srgb;
      default: {
        utility::logger<"scripting">::error("texture_load: invalid format {}", format);
        return graphics::format::r8g8b8a8_srgb;
      }
    }
  }();

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto texture = assets_module.load_texture(std::filesystem::path{std::string{path}}, native_format);

  // The record's handle, not the asset id: the same file loaded in another format is a separate record with the same id.
  return texture.is_valid() ? texture->handle().value() : 0u;
}

auto storage_image_format(std::uint32_t format) -> std::optional<graphics::format> {
  switch (format) {
    case 0u: return graphics::format::r8g8b8a8_unorm;
    case 1u: return graphics::format::r32_sfloat;
    case 2u: return graphics::format::r8_unorm;
    default: return std::nullopt;
  }
}

auto interop::texture_create_storage_image(std::uint32_t width, std::uint32_t height, std::uint32_t format) -> std::uint64_t {
  auto native_format = storage_image_format(format);

  if (!native_format) {
    utility::logger<"scripting">::error("texture_create_storage_image: invalid format {}", format);
    native_format = graphics::format::r8g8b8a8_unorm;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto texture = assets_module.create_storage_image(width, height, *native_format);

  return texture.is_valid() ? texture->id().value() : 0u;
}

auto interop::texture_read_pixels(std::uint64_t texture_uuid, std::uint32_t width, std::uint32_t height, std::uint32_t format, math::color* out_pixels) -> void {
  if (!out_pixels) {
    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto texture = assets_module.find_texture(math::uuid::from_value(texture_uuid));

  const auto source_image_handle = assets_module.image_handle_for(texture);

  if (!source_image_handle.is_valid()) {
    utility::logger<"scripting">::error("texture_read_pixels: no resident image for texture {}", texture_uuid);
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  auto& source_image = registry.get<graphics::image>(source_image_handle);

  // Only storage images live in `general`, the layout the copy below reads from.
  if ((source_image.usage() & graphics::image_usage::storage) != graphics::image_usage::storage) {
    utility::logger<"scripting">::error("texture_read_pixels: texture {} is not a storage image (only CreateStorageImage textures can be read back)", texture_uuid);
    return;
  }

  const auto& extent = source_image.extent();

  if (width != extent.x() || height != extent.y()) {
    utility::logger<"scripting">::error("texture_read_pixels: requested {}x{} but texture {} is {}x{}", width, height, texture_uuid, extent.x(), extent.y());
    return;
  }

  if (storage_image_format(format) != source_image.format()) {
    utility::logger<"scripting">::error("texture_read_pixels: format {} does not match texture {}'s format", format, texture_uuid);
    return;
  }

  const auto bytes_per_pixel = std::size_t{format == 2u ? 1u : 4u}; // R8 is 1 byte; RGBA8 and R32 float are 4

  const auto byte_size = static_cast<graphics::buffer::size_type>(width) * static_cast<graphics::buffer::size_type>(height) * bytes_per_pixel;

  const auto staging_handle = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = byte_size,
    .usage = graphics::buffer_usage::transfer_destination,
    .memory = graphics::memory_usage::host_read,
    .name = "Texture Readback Staging"
  });

  auto& staging = registry.get<graphics::buffer>(staging_handle);

  auto command_buffer = graphics::command_buffer{graphics::queue::type::compute, true};

  auto region = VkBufferImageCopy{};
  region.bufferOffset = 0u;
  region.bufferRowLength = 0u;
  region.bufferImageHeight = 0u;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0u;
  region.imageSubresource.baseArrayLayer = 0u;
  region.imageSubresource.layerCount = 1u;
  region.imageOffset = VkOffset3D{0, 0, 0};
  region.imageExtent = VkExtent3D{width, height, 1u};

  vkCmdCopyImageToBuffer(command_buffer.handle(), source_image.handle(), VK_IMAGE_LAYOUT_GENERAL, staging.handle(), 1u, &region);

  command_buffer.submit_idle();

  const auto* source = static_cast<const std::byte*>(staging.mapped());

  for (auto y = std::uint32_t{0u}; y < height; ++y) {
    for (auto x = std::uint32_t{0u}; x < width; ++x) {
      const auto pixel_index = static_cast<std::size_t>(y) * width + x;
      const auto* pixel = source + pixel_index * bytes_per_pixel;

      switch (format) {
        case 2u: { // R8 unorm
          const auto value = static_cast<std::float_t>(*reinterpret_cast<const std::uint8_t*>(pixel)) / 255.0f;
          out_pixels[pixel_index] = math::color{value, 0.0f, 0.0f, 1.0f};
          break;
        }
        case 1u: { // R32 float
          auto value = std::float_t{};
          std::memcpy(&value, pixel, sizeof(value));
          out_pixels[pixel_index] = math::color{value, 0.0f, 0.0f, 1.0f};
          break;
        }
        default: { // RGBA8 unorm
          const auto* rgba = reinterpret_cast<const std::uint8_t*>(pixel);
          out_pixels[pixel_index] = math::color{rgba[0] / 255.0f, rgba[1] / 255.0f, rgba[2] / 255.0f, rgba[3] / 255.0f};
          break;
        }
      }
    }
  }

  // retire() requires a non-decreasing timeline across the whole pool, so use the real frame index even though the buffer is already idle.
  registry.retire(staging_handle, graphics_module.frame_context().frame_index());
}

auto interop::debug_write_png(managed::string path, std::uint32_t width, std::uint32_t height, const std::uint8_t* rgba_pixels) -> managed::bool32 {
  if (!rgba_pixels || width == 0u || height == 0u) {
    return false;
  }

  const auto path_string = std::string{path};

  if (const auto parent = std::filesystem::path{path_string}.parent_path(); !parent.empty()) {
    std::filesystem::create_directories(parent);
  }

  const auto result = stbi_write_png(path_string.c_str(), static_cast<int>(width), static_cast<int>(height), 4, rgba_pixels, static_cast<int>(width) * 4);

  if (result == 0) {
    utility::logger<"scripting">::error("debug_write_png: failed to write '{}'", path_string);
    return false;
  }

  return true;
}

auto interop::texture_release(std::uint64_t texture_uuid) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto texture = assets_module.find_texture(math::uuid::from_value(texture_uuid));

  assets_module.release_texture(texture);
}

auto interop::texture2d_array_create(std::uint64_t* layer_uuids, std::uint32_t layer_count, std::uint32_t width, std::uint32_t height) -> std::uint64_t {
  if (!layer_uuids) {
    return 0u;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto layers = std::vector<assets::texture2d_handle>{};
  layers.reserve(layer_count);

  for (auto i = std::uint32_t{0u}; i < layer_count; ++i) {
    layers.push_back(assets_module.find_texture(math::uuid::from_value(layer_uuids[i])));
  }

  auto array = assets_module.create_texture2d_array(layers, math::vector2u{width, height});

  return array.is_valid() ? array->id().value() : 0u;
}

auto interop::texture2d_array_is_resident(std::uint64_t array_uuid) -> managed::bool32 {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  return assets_module.is_resident(assets_module.find_texture2d_array(math::uuid::from_value(array_uuid)));
}

auto interop::texture2d_array_release(std::uint64_t array_uuid) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  assets_module.release_texture2d_array(assets_module.find_texture2d_array(math::uuid::from_value(array_uuid)));
}

auto interop::material_release(std::uint64_t material_uuid) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto material = assets_module.load_material(math::uuid::from_value(material_uuid));

  assets_module.release_material(material);
}

auto interop::texture_is_resident(std::uint64_t texture_uuid) -> managed::bool32 {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto texture = assets_module.find_texture(math::uuid::from_value(texture_uuid));

  return assets_module.is_resident(texture);
}

auto interop::font_load(managed::string path) -> std::uint64_t {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto font = assets_module.load_font(std::filesystem::path{std::string{path}});

  return font.is_valid() ? font->id().value() : 0u;
}

auto interop::font_is_resident(std::uint64_t font_uuid) -> managed::bool32 {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto font = assets_module.load_font(math::uuid::from_value(font_uuid));

  return font.is_valid() && assets_module.is_resident(font);
}

auto interop::font_get_glyph(std::uint64_t font_uuid, std::uint32_t codepoint, font_glyph_data* out) -> managed::bool32 {
  if (!out) {
    return false;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto font = assets_module.load_font(math::uuid::from_value(font_uuid));

  if (!font.is_valid()) {
    return false;
  }

  const auto glyph = font->glyph_for(codepoint);

  if (!glyph) {
    return false;
  }

  *out = font_glyph_data{
    glyph->uv_rect.x(), glyph->uv_rect.y(), glyph->uv_rect.z(), glyph->uv_rect.w(),
    glyph->width, glyph->height, glyph->bearing_x, glyph->bearing_y, glyph->advance
  };

  return true;
}

auto interop::font_get_metrics(std::uint64_t font_uuid, math::vector3* out) -> void {
  if (!out) {
    return;
  }

  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto font = assets_module.load_font(math::uuid::from_value(font_uuid));

  *out = font.is_valid() ? math::vector3{font->line_height(), font->ascent(), font->descent()} : math::vector3{0.0f, 0.0f, 0.0f};
}

auto interop::material_set_generic_texture_font(std::uint64_t material_uuid, std::uint32_t index, std::uint64_t font_uuid) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto material = assets_module.load_material(math::uuid::from_value(material_uuid));
  auto font = assets_module.load_font(math::uuid::from_value(font_uuid));

  if (!material.is_valid() || !font.is_valid()) {
    return;
  }

  if (index >= assets::shader_graph_max_textures) {
    utility::logger<"scripting">::error("material_set_generic_texture_font: index {} out of range (max {})", index, assets::shader_graph_max_textures);
    return;
  }

  auto create_info = material->to_create_info();
  create_info.generic_textures[index] = font->atlas();

  assets_module.update_material(material, create_info);
}

auto interop::material_is_loaded(std::uint64_t material_uuid) -> managed::bool32 {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto material = assets_module.load_material(math::uuid::from_value(material_uuid));

  return material.is_loaded();
}

auto interop::material_create_instance(std::uint64_t source_uuid) -> std::uint64_t {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto source = assets_module.load_material(math::uuid::from_value(source_uuid));
  auto duplicated = assets_module.duplicate_material(source);

  return duplicated.is_valid() ? duplicated->id().value() : 0u;
}

auto interop::material_set_texture(std::uint64_t material_uuid, std::uint32_t slot, std::uint64_t texture_uuid) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto material = assets_module.load_material(math::uuid::from_value(material_uuid));

  if (!material.is_valid()) {
    return;
  }

  auto texture = assets_module.find_texture(math::uuid::from_value(texture_uuid));

  auto create_info = material->to_create_info();

  switch (slot) {
    case 0u: create_info.albedo = texture; break;
    case 1u: create_info.normal = texture; break;
    case 2u: create_info.metallic_roughness = texture; break;
    case 3u: create_info.occlusion = texture; break;
    case 4u: create_info.emissive = texture; break;
    default: {
      utility::logger<"scripting">::error("material_set_texture: invalid slot {}", slot);
      return;
    }
  }

  assets_module.update_material(material, create_info);
}

auto interop::material_set_generic_param(std::uint64_t material_uuid, std::uint32_t index, std::float_t x, std::float_t y, std::float_t z, std::float_t w) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto material = assets_module.load_material(math::uuid::from_value(material_uuid));

  if (!material.is_valid()) {
    return;
  }

  if (index >= assets::shader_graph_max_params) {
    utility::logger<"scripting">::error("material_set_generic_param: index {} out of range (max {})", index, assets::shader_graph_max_params);
    return;
  }

  auto create_info = material->to_create_info();
  create_info.generic_params[index] = math::vector4{x, y, z, w};

  assets_module.update_material(material, create_info);
}

auto interop::material_set_generic_texture_array(std::uint64_t material_uuid, std::uint32_t index, std::uint64_t array_uuid) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto material = assets_module.load_material(math::uuid::from_value(material_uuid));

  if (!material.is_valid()) {
    return;
  }

  if (index >= assets::shader_graph_max_textures) {
    utility::logger<"scripting">::error("material_set_generic_texture_array: index {} out of range (max {})", index, assets::shader_graph_max_textures);
    return;
  }

  auto create_info = material->to_create_info();
  create_info.generic_texture_arrays[index] = assets_module.find_texture2d_array(math::uuid::from_value(array_uuid));

  assets_module.update_material(material, create_info);
}

auto interop::material_set_generic_texture(std::uint64_t material_uuid, std::uint32_t index, std::uint64_t texture_uuid) -> void {
  auto& assets_module = core::engine::get_module<assets::assets_module>();
  auto material = assets_module.load_material(math::uuid::from_value(material_uuid));

  if (!material.is_valid()) {
    return;
  }

  if (index >= assets::shader_graph_max_textures) {
    utility::logger<"scripting">::error("material_set_generic_texture: index {} out of range (max {})", index, assets::shader_graph_max_textures);
    return;
  }

  auto create_info = material->to_create_info();
  create_info.generic_textures[index] = assets_module.find_texture(math::uuid::from_value(texture_uuid));
  create_info.generic_texture_arrays[index] = assets::texture2d_array_handle{};

  assets_module.update_material(material, create_info);
}

} // namespace sbx::scripting
