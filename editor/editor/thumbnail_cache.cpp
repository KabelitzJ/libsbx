// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/thumbnail_cache.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <string_view>

#include <vulkan/vulkan.h>

#include <yaml-cpp/yaml.h>

#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/memory/bytes.hpp>
#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/resource_registry.hpp>
#include <libsbx/graphics/resources/sampler.hpp>
#include <libsbx/graphics/bindless_table.hpp>

#include <libsbx/render/ui/ui_module.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/primitive_meshes.hpp>

#include <libsbx/scenes/components.hpp>

namespace editor {

/** @brief Mirrors the thumbnail shader's `push_data`; must stay byte-identical and within the shared 128-byte budget. */
struct thumbnail_push_constants {
  sbx::math::matrix4x4 view_projection;
  sbx::math::vector4 light_direction; // xyz = direction (surface <- light)
  sbx::graphics::buffer::address_type vertex_address;
  sbx::graphics::buffer::address_type materials_address;
  std::uint32_t material_index;
  std::uint32_t sampler_index;
}; // struct thumbnail_push_constants

static_assert(sizeof(thumbnail_push_constants) <= 128u, "Thumbnail push constants must fit the engine's shared 128-byte push-constant budget.");

// One light, albedo x base colour plus emission; enough to tell assets apart at tile size.
inline constexpr auto thumbnail_shader_source = std::string_view{R"(#include <frame_data.slang>

struct push_data {
  float4x4 view_projection;
  float4 light_direction;
  vertex* vertices;
  material_data* materials;
  uint material_index;
  uint sampler_index;
}; // struct push_data

[[vk::push_constant]] ConstantBuffer<push_data> push;

struct vertex_output {
  [[vk::location(0)]] float3 normal : NORMAL;
  [[vk::location(1)]] float2 uv : TEXCOORD;
  float4 sv_position : SV_Position;
}; // struct vertex_output

[shader("vertex")]
vertex_output vertex_main(uint vertex_id : SV_VertexID) {
  vertex v = push.vertices[vertex_id];

  vertex_output output;
  output.normal = v.normal;
  output.uv = v.uv;
  output.sv_position = mul(push.view_projection, float4(v.position, 1.0));

  return output;
}

[shader("fragment")]
float4 fragment_main(vertex_output input) : SV_Target0 {
  material_data material = push.materials[push.material_index];

  float2 uv = apply_uv_transform(material, input.uv);
  float3 albedo = material.base_color_factor.rgb * textures[material.albedo_index].Sample(samplers[push.sampler_index], uv).rgb;

  float n_dot_l = max(dot(normalize(input.normal), normalize(-push.light_direction.xyz)), 0.0);
  float3 color = albedo * (0.25 + 0.9 * n_dot_l) + material.emissive_factor.rgb * material.emissive_strength;

  // Linear out: the sRGB target encodes on write and decodes on sample, and the sRGB swapchain encodes once more for display.
  return float4(saturate(color), 1.0);
}
)"};

thumbnail_cache::thumbnail_cache() = default;

thumbnail_cache::~thumbnail_cache() {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  const auto frame_index = graphics_module.frame_context().frame_index();

  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  for (auto& [id, entry] : _entries) {
    if (entry.image.is_valid()) {
      ui_module.release_texture_id(registry.get<sbx::graphics::image>(entry.image).view());
      registry.retire(entry.image, frame_index);
    }
  }

  if (_depth.is_valid()) {
    registry.retire(_depth, frame_index);
  }
}

auto thumbnail_cache::supports(sbx::assets::asset_type type) -> bool {
  return type == sbx::assets::asset_type::mesh || type == sbx::assets::asset_type::material || type == sbx::assets::asset_type::prefab;
}

auto thumbnail_cache::get(const sbx::math::uuid& id, sbx::assets::asset_type type) -> std::optional<ImTextureID> {
  if (!supports(type) || id == sbx::math::uuid::nil()) {
    return std::nullopt;
  }

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  auto [iterator, is_new] = _entries.try_emplace(id);
  auto& entry = iterator->second;

  if (is_new) {
    if (type == sbx::assets::asset_type::mesh) {
      entry.parts.push_back(part{.mesh = assets_module.load_mesh(id)});
    } else if (type == sbx::assets::asset_type::material) {
      sbx::assets::ensure_primitive_mesh_cooked(sbx::assets::primitive_mesh_kind::sphere);
      entry.parts.push_back(part{.mesh = assets_module.load_mesh(sbx::assets::primitive_mesh_uuid(sbx::assets::primitive_mesh_kind::sphere))});
      entry.material_override = assets_module.load_material(id);
    } else {
      entry.parts = _prefab_parts(id);
    }

    std::erase_if(entry.parts, [](const auto& part) { return !part.mesh.is_valid(); });
  }

  entry.last_used = ++_use_counter;

  if (!entry.is_queued && !entry.parts.empty() && _content_key(entry) != entry.rendered_key) {
    entry.is_queued = true;
    _queue.push_back(id);
  }

  if (!entry.image.is_valid()) {
    return std::nullopt;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();

  return ui_module.texture_id(graphics_module.resource_registry().get<sbx::graphics::image>(entry.image).view(), ui_module.thumbnail_sampler());
}

auto thumbnail_cache::_prefab_parts(const sbx::math::uuid& id) -> std::vector<part> {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto prefab = assets_module.load_prefab(id);

  if (!prefab.is_valid()) {
    return {};
  }

  const auto& snapshot = prefab->snapshot();

  // The snapshot names assets by short keys; its assets table maps them to uuids.
  auto key_to_uuid = std::unordered_map<std::string, sbx::math::uuid>{};

  for (const auto category : {std::string{"static_meshes"}, std::string{"materials"}}) {
    if (const auto table = snapshot["assets"][category]; table && table.IsSequence()) {
      for (const auto asset : table) {
        key_to_uuid.emplace(asset["key"].as<std::string>(), sbx::math::uuid::from_value(asset["uuid"].as<std::uint64_t>()));
      }
    }
  }

  const auto uuid_of = [&key_to_uuid](const YAML::Node& key) {
    const auto entry = key_to_uuid.find(key.as<std::string>());
    return entry != key_to_uuid.end() ? entry->second : sbx::math::uuid::nil();
  };

  struct node_info {
    std::uint64_t parent{0u};
    bool has_parent{false};
    sbx::math::matrix4x4 local{sbx::math::matrix4x4::identity};
  }; // struct node_info

  auto nodes = std::unordered_map<std::uint64_t, node_info>{};
  auto parts = std::vector<std::pair<std::uint64_t, part>>{};

  for (const auto node_yaml : snapshot["nodes"]) {
    const auto node_id = node_yaml["id"].as<std::uint64_t>();
    auto& info = nodes[node_id];

    if (const auto parent = node_yaml["parent"]) {
      info.parent = parent.as<std::uint64_t>();
      info.has_parent = true;
    }

    for (const auto component : node_yaml["components"]) {
      const auto type = component["type"].as<std::string>();

      if (type == "transform") {
        info.local = sbx::scenes::local_transform{
          .position = component["position"].as<sbx::math::vector3>(),
          .rotation = component["rotation"].as<sbx::math::quaternion>(),
          .scale = component["scale"].as<sbx::math::vector3>()
        }.matrix();
      } else if (type == "static_mesh") {
        auto mesh_part = part{.mesh = assets_module.load_mesh(uuid_of(component["mesh"]))};

        if (const auto submeshes = component["submeshes"]) {
          for (const auto submesh : submeshes) {
            const auto index = submesh["index"].as<std::size_t>();

            if (mesh_part.materials.size() <= index) {
              mesh_part.materials.resize(index + 1u);
            }

            mesh_part.materials[index] = assets_module.load_material(uuid_of(submesh["material"]));
          }
        }

        parts.emplace_back(node_id, std::move(mesh_part));
      }
    }
  }

  // Relative to the prefab root: the root's own transform is where the prefab was placed when it was made, not part of its look.
  const auto world_of = [&nodes](std::uint64_t node_id) {
    auto matrix = sbx::math::matrix4x4::identity;

    for (auto current = nodes.find(node_id); current != nodes.end() && current->second.has_parent; current = nodes.find(current->second.parent)) {
      matrix = current->second.local * matrix;
    }

    return matrix;
  };

  auto result = std::vector<part>{};
  result.reserve(parts.size());

  for (auto& [node_id, mesh_part] : parts) {
    mesh_part.model = world_of(node_id);
    result.push_back(std::move(mesh_part));
  }

  return result;
}

auto thumbnail_cache::_material_of(const entry& entry, const part& part, std::size_t submesh) -> sbx::assets::material_handle {
  if (entry.material_override.is_valid()) {
    return entry.material_override;
  }

  if (submesh < part.materials.size() && part.materials[submesh].is_valid()) {
    return part.materials[submesh];
  }

  return part.mesh->submeshes()[submesh].material;
}

// Changes whenever a mesh or any material it's drawn with reloads, so reimports and material edits re-render.
auto thumbnail_cache::_content_key(const entry& entry) const -> std::uint64_t {
  auto seed = std::size_t{0u};

  for (const auto& part : entry.parts) {
    sbx::utility::hash_combine(seed, part.mesh.generation());

    if (!part.mesh->is_loaded()) {
      continue;
    }

    for (auto submesh = std::size_t{0u}; submesh < part.mesh->submeshes().size(); ++submesh) {
      if (const auto material = _material_of(entry, part, submesh); material.is_valid()) {
        sbx::utility::hash_combine(seed, material.generation());
      }
    }
  }

  // 0 is reserved for "never rendered".
  return seed == 0u ? 1u : seed;
}

auto thumbnail_cache::_is_ready(const entry& entry) const -> bool {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto is_material_ready = [&](const sbx::assets::material_handle& material) {
    return !material.is_valid() || (material->is_loaded() && (!material->albedo().is_valid() || assets_module.is_resident(material->albedo())));
  };

  return std::ranges::all_of(entry.parts, [&](const auto& part) {
    if (!part.mesh->is_loaded() || part.mesh->vertex_address() == 0u || part.mesh->submeshes().empty()) {
      return false;
    }

    for (auto submesh = std::size_t{0u}; submesh < part.mesh->submeshes().size(); ++submesh) {
      if (!is_material_ready(_material_of(entry, part, submesh))) {
        return false;
      }
    }

    return true;
  });
}

auto thumbnail_cache::_release_image(entry& entry) -> void {
  if (!entry.image.is_valid()) {
    return;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  sbx::core::engine::get_module<sbx::render::ui_module>().release_texture_id(registry.get<sbx::graphics::image>(entry.image).view());
  registry.retire(entry.image, graphics_module.frame_context().frame_index());

  entry.image = sbx::graphics::image_handle{};
}

auto thumbnail_cache::_evict_images(const sbx::math::uuid& keep) -> void {
  auto with_image = std::size_t{0u};

  for (const auto& [id, entry] : _entries) {
    with_image += entry.image.is_valid() ? 1u : 0u;
  }

  while (with_image > max_images) {
    auto oldest = _entries.end();

    for (auto iterator = _entries.begin(); iterator != _entries.end(); ++iterator) {
      if (iterator->first != keep && iterator->second.image.is_valid() && (oldest == _entries.end() || iterator->second.last_used < oldest->second.last_used)) {
        oldest = iterator;
      }
    }

    if (oldest == _entries.end()) {
      return;
    }

    _release_image(oldest->second);

    // Re-renders the next time it's shown.
    oldest->second.rendered_key = 0u;
    --with_image;
  }
}

auto thumbnail_cache::update() -> void {
  if (_queue.empty() || !_ensure_pipeline()) {
    return;
  }

  // Not-ready entries rotate to the back, so one slow asset doesn't block the rest.
  for (auto attempts = _queue.size(); attempts > 0u; --attempts) {
    const auto id = _queue.front();
    _queue.pop_front();

    const auto found = _entries.find(id);

    if (found == _entries.end()) {
      continue;
    }

    auto& entry = found->second;

    if (!_is_ready(entry)) {
      _queue.push_back(id);
      continue;
    }

    entry.is_queued = false;
    _render(id, entry);

    return;
  }
}

auto thumbnail_cache::_ensure_pipeline() -> bool {
  if (_pipeline) {
    return true;
  }

  if (!_compile_requested) {
    auto request = sbx::graphics::async_shader_compiler::request{};
    request.key = 0u;
    request.path = sbx::filesystem::engine_data_directory() / "shaders" / "generated" / "preview" / "asset_thumbnail.slang";
    request.source = std::string{thumbnail_shader_source};
    request.entry_points = std::vector<sbx::graphics::shader_compiler::entry_point_request>{
      sbx::graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
      sbx::graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
    };

    _compiler.submit(std::move(request));
    _compile_requested = true;
  }

  for (auto& result : _compiler.take_results(1u)) {
    if (!result.success) {
      sbx::utility::logger<"editor">::error("Asset thumbnail shader failed to compile: {}", result.error);
      continue;
    }

    _shader = std::make_unique<sbx::graphics::shader>(std::move(result.compiled), 0u);

    _pipeline = std::make_unique<sbx::graphics::graphics_pipeline>(sbx::graphics::graphics_pipeline::create_info{
      .shader = sbx::memory::make_observer<const sbx::graphics::shader>(_shader.get()),
      .color_formats = {sbx::graphics::format::r8g8b8a8_srgb},
      .depth_format = sbx::graphics::format::d32_sfloat,
      .cull_mode = sbx::graphics::cull_mode::none,
      .depth_test = true,
      .depth_write = true,
      .samples = sbx::graphics::samples::count_1,
      .name = "Asset Thumbnail"
    });

    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

    _depth = graphics_module.resource_registry().emplace<sbx::graphics::image>(sbx::graphics::image::create_info{
      .extent = {_extent, _extent, 1u},
      .format = sbx::graphics::format::d32_sfloat,
      .usage = sbx::graphics::image_usage::depth_stencil_attachment,
      .name = "Asset Thumbnail Depth"
    });

    _sampler_index = graphics_module.bindless_table().sampler_index(sbx::graphics::sampler::create_info{
      .max_anisotropy = 16.0f,
      .name = "Asset Thumbnail Sampler"
    });
  }

  return _pipeline != nullptr;
}

auto thumbnail_cache::_render(const sbx::math::uuid& id, entry& entry) -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  // A fresh image per render: frames still in flight may be sampling the previous one in ImGui.
  _release_image(entry);

  entry.image = registry.emplace<sbx::graphics::image>(sbx::graphics::image::create_info{
    .extent = {_extent, _extent, 1u},
    .format = sbx::graphics::format::r8g8b8a8_srgb,
    .usage = sbx::graphics::image_usage::color_attachment | sbx::graphics::image_usage::sampled,
    .name = "Asset Thumbnail"
  });

  entry.rendered_key = _content_key(entry);

  _evict_images(id);

  const auto& target = registry.get<sbx::graphics::image>(entry.image);
  const auto& depth = registry.get<sbx::graphics::image>(_depth);

  // Every part's bounds, transformed corner by corner into the thumbnail's space.
  auto bounds_min = sbx::math::vector3{std::numeric_limits<std::float_t>::max()};
  auto bounds_max = sbx::math::vector3{std::numeric_limits<std::float_t>::lowest()};

  for (const auto& part : entry.parts) {
    const auto& local = part.mesh->bounds();

    for (auto corner = 0u; corner < 8u; ++corner) {
      const auto point = sbx::math::vector3{
        (corner & 1u) != 0u ? local.max().x() : local.min().x(),
        (corner & 2u) != 0u ? local.max().y() : local.min().y(),
        (corner & 4u) != 0u ? local.max().z() : local.min().z()
      };

      const auto homogeneous = part.model * sbx::math::vector4{point, 1.0f};
      const auto transformed = sbx::math::vector3{homogeneous.x(), homogeneous.y(), homogeneous.z()};

      bounds_min = sbx::math::vector3::min(bounds_min, transformed);
      bounds_max = sbx::math::vector3::max(bounds_max, transformed);
    }
  }

  // Frames the bounds from a three-quarter view, like a Unity preview.
  const auto center = (bounds_min + bounds_max) * 0.5f;
  const auto radius = std::max((bounds_max - bounds_min).length() * 0.5f, 0.001f);
  const auto eye = center + sbx::math::vector3::normalized(sbx::math::vector3{1.0f, 0.8f, 1.2f}) * (radius * 2.8f);

  const auto view = sbx::math::matrix4x4::look_at(eye, center, sbx::math::vector3{0.0f, 1.0f, 0.0f});
  const auto projection = sbx::math::matrix4x4::perspective(sbx::math::degree{40.0f}, 1.0f, radius * 0.05f, radius * 8.0f);

  const auto view_projection = projection * view;
  const auto light_direction = sbx::math::vector4{-0.4f, -0.8f, -0.5f, 0.0f};

  auto push = thumbnail_push_constants{};
  push.materials_address = assets_module.material_buffer_address();
  push.sampler_index = _sampler_index;

  auto command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

  {
    auto to_attachment = sbx::graphics::command_buffer::image_transition_data{};
    to_attachment.image = target.handle();
    to_attachment.src_stage_mask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    to_attachment.src_access_mask = 0u;
    to_attachment.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_attachment.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    to_attachment.old_layout = sbx::graphics::image_layout::undefined;
    to_attachment.new_layout = sbx::graphics::image_layout::color_attachment_optimal;
    command_buffer.transition_image_layout(to_attachment);

    auto depth_to_attachment = sbx::graphics::command_buffer::image_transition_data{};
    depth_to_attachment.image = depth.handle();
    depth_to_attachment.src_stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    depth_to_attachment.src_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depth_to_attachment.dst_stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    depth_to_attachment.dst_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depth_to_attachment.old_layout = sbx::graphics::image_layout::undefined;
    depth_to_attachment.new_layout = sbx::graphics::image_layout::depth_stencil_attachment_optimal;
    depth_to_attachment.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
    command_buffer.transition_image_layout(depth_to_attachment);
  }

  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = target.view();
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.clearValue.color = VkClearColorValue{{0.16f, 0.16f, 0.19f, 1.0f}};

  auto depth_attachment = VkRenderingAttachmentInfo{};
  depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depth_attachment.imageView = depth.view();
  depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.clearValue.depthStencil = VkClearDepthStencilValue{1.0f, 0u};

  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{_extent, _extent}};
  rendering_info.layerCount = 1u;
  rendering_info.colorAttachmentCount = 1u;
  rendering_info.pColorAttachments = &color_attachment;
  rendering_info.pDepthAttachment = &depth_attachment;

  command_buffer.begin_rendering(rendering_info);

  command_buffer.set_viewport(VkViewport{0.0f, 0.0f, static_cast<std::float_t>(_extent), static_cast<std::float_t>(_extent), 0.0f, 1.0f});
  command_buffer.set_scissor(VkRect2D{VkOffset2D{0, 0}, VkExtent2D{_extent, _extent}});

  command_buffer.bind_pipeline(*_pipeline);

  const auto& bindless_table = graphics_module.bindless_table();
  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  const auto fallback_material = assets_module.load_material(sbx::assets::default_material_uuid());

  for (const auto& part : entry.parts) {
    const auto& mesh = *part.mesh;

    // The shader has no model matrix: it's folded into the view-projection, and the light is moved into the part's space instead of the normals into the world.
    push.view_projection = view_projection * part.model;
    push.light_direction = sbx::math::matrix4x4::inverted(part.model) * light_direction;
    push.vertex_address = mesh.vertex_address();

    command_buffer.bind_index_buffer(registry.get<sbx::graphics::buffer>(mesh.index_buffer()), 0u, VK_INDEX_TYPE_UINT32);

    for (auto index = std::size_t{0u}; index < mesh.submeshes().size(); ++index) {
      const auto& submesh = mesh.submeshes()[index];
      const auto material = _material_of(entry, part, index);
      const auto& resolved = material.is_valid() ? material : fallback_material;

      push.material_index = resolved.is_valid() ? resolved->index() : 0u;

      command_buffer.push_constants(bindless_table.pipeline_layout(), sbx::graphics::bindless_table::push_constant_stages, 0u, sbx::memory::as_bytes(push));
      command_buffer.draw_indexed(submesh.index_count, 1u, submesh.index_offset, 0, 0u);
    }
  }

  command_buffer.end_rendering();

  {
    auto to_read = sbx::graphics::command_buffer::image_transition_data{};
    to_read.image = target.handle();
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_read.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = sbx::graphics::image_layout::color_attachment_optimal;
    to_read.new_layout = sbx::graphics::image_layout::shader_read_only_optimal;
    command_buffer.transition_image_layout(to_read);
  }

  command_buffer.submit_idle();
}

} // namespace editor
