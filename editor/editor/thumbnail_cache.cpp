// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/thumbnail_cache.hpp>

#include <algorithm>
#include <cmath>
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
      entry.mesh = assets_module.load_mesh(id);
    } else if (type == sbx::assets::asset_type::material) {
      sbx::assets::ensure_primitive_mesh_cooked(sbx::assets::primitive_mesh_kind::sphere);
      entry.mesh = assets_module.load_mesh(sbx::assets::primitive_mesh_uuid(sbx::assets::primitive_mesh_kind::sphere));
      entry.material_override = assets_module.load_material(id);
    } else if (const auto prefab = assets_module.load_prefab(id); prefab.is_valid()) {
      // ponytail: shows the prefab's first mesh with that mesh's own materials; compose every mesh_renderer with its transform if multi-part prefabs read badly.
      if (const auto meshes = prefab->snapshot()["assets"]["static_meshes"]; meshes && meshes.IsSequence() && meshes.size() > 0u) {
        entry.mesh = assets_module.load_mesh(sbx::math::uuid::from_value(meshes[0]["uuid"].as<std::uint64_t>()));
      }
    }
  }

  if (!entry.is_queued && entry.mesh.is_valid() && _content_key(entry) != entry.rendered_key) {
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

// Changes whenever the mesh or any material it's drawn with reloads, so reimports and material edits re-render.
auto thumbnail_cache::_content_key(const entry& entry) const -> std::uint64_t {
  auto seed = std::size_t{entry.mesh.generation()};

  if (entry.material_override.is_valid()) {
    sbx::utility::hash_combine(seed, entry.material_override.generation());
  } else if (entry.mesh->is_loaded()) {
    for (const auto& submesh : entry.mesh->submeshes()) {
      if (submesh.material.is_valid()) {
        sbx::utility::hash_combine(seed, submesh.material.generation());
      }
    }
  }

  // 0 is reserved for "never rendered".
  return seed == 0u ? 1u : seed;
}

auto thumbnail_cache::_is_ready(const entry& entry) const -> bool {
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  if (!entry.mesh->is_loaded() || entry.mesh->vertex_address() == 0u || entry.mesh->submeshes().empty()) {
    return false;
  }

  const auto is_material_ready = [&](const sbx::assets::material_handle& material) {
    return !material.is_valid() || (material->is_loaded() && (!material->albedo().is_valid() || assets_module.is_resident(material->albedo())));
  };

  if (entry.material_override.is_valid()) {
    return is_material_ready(entry.material_override);
  }

  return std::ranges::all_of(entry.mesh->submeshes(), [&](const auto& submesh) { return is_material_ready(submesh.material); });
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
    _render(entry);

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

auto thumbnail_cache::_render(entry& entry) -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  // A fresh image per render: frames still in flight may be sampling the previous one in ImGui.
  if (entry.image.is_valid()) {
    sbx::core::engine::get_module<sbx::render::ui_module>().release_texture_id(registry.get<sbx::graphics::image>(entry.image).view());
    registry.retire(entry.image, graphics_module.frame_context().frame_index());
  }

  // ponytail: one 64 KiB image per thumbnail, never evicted; add an LRU cap if huge projects run out of memory.
  entry.image = registry.emplace<sbx::graphics::image>(sbx::graphics::image::create_info{
    .extent = {_extent, _extent, 1u},
    .format = sbx::graphics::format::r8g8b8a8_srgb,
    .usage = sbx::graphics::image_usage::color_attachment | sbx::graphics::image_usage::sampled,
    .name = "Asset Thumbnail"
  });

  entry.rendered_key = _content_key(entry);

  const auto& mesh = *entry.mesh;
  const auto& target = registry.get<sbx::graphics::image>(entry.image);
  const auto& depth = registry.get<sbx::graphics::image>(_depth);

  // Frames the bounds from a three-quarter view, like a Unity preview.
  const auto& bounds = mesh.bounds();
  const auto center = bounds.center();
  const auto radius = std::max((bounds.max() - bounds.min()).length() * 0.5f, 0.001f);
  const auto eye = center + sbx::math::vector3::normalized(sbx::math::vector3{1.0f, 0.8f, 1.2f}) * (radius * 2.8f);

  const auto view = sbx::math::matrix4x4::look_at(eye, center, sbx::math::vector3{0.0f, 1.0f, 0.0f});
  const auto projection = sbx::math::matrix4x4::perspective(sbx::math::degree{40.0f}, 1.0f, radius * 0.05f, radius * 8.0f);

  auto push = thumbnail_push_constants{};
  push.view_projection = projection * view;
  push.light_direction = sbx::math::vector4{-0.4f, -0.8f, -0.5f, 0.0f};
  push.vertex_address = mesh.vertex_address();
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

  command_buffer.bind_index_buffer(registry.get<sbx::graphics::buffer>(mesh.index_buffer()), 0u, VK_INDEX_TYPE_UINT32);

  const auto& bindless_table = graphics_module.bindless_table();
  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  const auto fallback_material = assets_module.load_material(sbx::assets::default_material_uuid());

  for (const auto& submesh : mesh.submeshes()) {
    const auto& material = entry.material_override.is_valid() ? entry.material_override : (submesh.material.is_valid() ? submesh.material : fallback_material);

    push.material_index = material.is_valid() ? material->index() : 0u;

    command_buffer.push_constants(bindless_table.pipeline_layout(), sbx::graphics::bindless_table::push_constant_stages, 0u, sbx::memory::as_bytes(push));
    command_buffer.draw_indexed(submesh.index_count, 1u, submesh.index_offset, 0, 0u);
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
