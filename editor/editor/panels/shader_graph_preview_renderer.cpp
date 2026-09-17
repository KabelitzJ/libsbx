// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/shader_graph_preview_renderer.hpp>

#include <algorithm>
#include <cmath>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/math/angle.hpp>
#include <libsbx/math/color.hpp>
#include <libsbx/math/vector3.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/resource_registry.hpp>
#include <libsbx/graphics/resources/sampler.hpp>
#include <libsbx/graphics/bindless_table.hpp>

#include <libsbx/memory/observer_ptr.hpp>
#include <libsbx/memory/bytes.hpp>

#include <libsbx/render/ui/ui_module.hpp>

#include <libsbx/assets/assets_module.hpp>
#include <libsbx/assets/primitive_meshes.hpp>
#include <libsbx/assets/shader_graph_codegen.hpp>

namespace editor {

shader_graph_preview_renderer::shader_graph_preview_renderer() {

}

shader_graph_preview_renderer::~shader_graph_preview_renderer() {
  if (!_resources_ready) {
    return;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  auto& frame_context = graphics_module.frame_context();
  const auto frame_index = frame_context.frame_index();

  registry.retire(_target, frame_index);
  registry.retire(_material_buffer, frame_index);
}

auto shader_graph_preview_renderer::_ensure_resources() -> void {
  if (_resources_ready) {
    return;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();

  _sampler_index = bindless_table.sampler_index(sbx::graphics::sampler::create_info{
    .max_anisotropy = 16.0f,
    .name = "Shader Graph Preview Sampler"
  });

  _target = registry.emplace<sbx::graphics::image>(sbx::graphics::image::create_info{
    .extent = {_extent, _extent, 1u},
    .format = sbx::graphics::format::r8g8b8a8_unorm,
    .usage = sbx::graphics::image_usage::color_attachment | sbx::graphics::image_usage::sampled,
    .name = "Shader Graph Master Preview"
  });

  _material_buffer = registry.emplace<sbx::graphics::buffer>(sbx::graphics::buffer::create_info{
    .size = sizeof(shader_graph_preview_material_data),
    .usage = sbx::graphics::buffer_usage::storage | sbx::graphics::buffer_usage::device_address,
    .memory = sbx::graphics::memory_usage::host_write,
    .name = "Shader Graph Master Preview Material"
  });

  _material_address = registry.get<sbx::graphics::buffer>(_material_buffer).address();

  sbx::assets::ensure_primitive_mesh_cooked(sbx::assets::primitive_mesh_kind::sphere);

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  _sphere = assets_module.load_mesh(sbx::assets::primitive_mesh_uuid(sbx::assets::primitive_mesh_kind::sphere));

  _resources_ready = true;
}

auto shader_graph_preview_renderer::request_recompile(const sbx::assets::shader_graph::create_info& graph) -> void {
  const auto source = sbx::assets::generate_shader_graph_preview_source("shader_graph_master_preview", graph);

  if (!source) {
    _error = source.error();
    return;
  }

  _error.clear();

  auto request = sbx::graphics::async_shader_compiler::request{};
  request.key = _key;
  request.path = sbx::filesystem::engine_data_directory() / "shaders" / "generated" / "preview" / "master_preview.slang";
  request.source = *source;
  request.entry_points = std::vector<sbx::graphics::shader_compiler::entry_point_request>{
    sbx::graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    sbx::graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  _compiler.submit(std::move(request));
}

auto shader_graph_preview_renderer::_refresh_material(const sbx::assets::shader_graph::create_info& graph) -> bool {
  auto data = shader_graph_preview_material_data{};

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  const auto white = assets_module.white_texture();

  for (const auto& parameter : sbx::assets::compute_shader_graph_parameters(graph.nodes)) {
    const auto entry = std::ranges::find(graph.nodes, parameter.node_id, &sbx::assets::shader_graph_node::id);

    if (entry == graph.nodes.end()) {
      continue;
    }

    switch (parameter.type) {
      case sbx::assets::shader_graph_parameter_type::float_value: {
        const auto value = std::holds_alternative<std::float_t>(entry->value) ? std::get<std::float_t>(entry->value) : 0.0f;
        data.generic_params[parameter.slot] = sbx::math::vector4{value, 0.0f, 0.0f, 0.0f};
        break;
      }
      case sbx::assets::shader_graph_parameter_type::vector3_value: {
        const auto value = std::holds_alternative<sbx::math::vector3>(entry->value) ? std::get<sbx::math::vector3>(entry->value) : sbx::math::vector3{};
        data.generic_params[parameter.slot] = sbx::math::vector4{value.x(), value.y(), value.z(), 0.0f};
        break;
      }
      case sbx::assets::shader_graph_parameter_type::color_value: {
        const auto value = std::holds_alternative<sbx::math::color>(entry->value) ? std::get<sbx::math::color>(entry->value) : sbx::math::color{1.0f, 1.0f, 1.0f, 1.0f};
        data.generic_params[parameter.slot] = sbx::math::vector4{value.r(), value.g(), value.b(), value.a()};
        break;
      }
      case sbx::assets::shader_graph_parameter_type::texture_value: {
        const auto handle = std::holds_alternative<sbx::assets::texture_handle>(entry->value) ? std::get<sbx::assets::texture_handle>(entry->value) : sbx::assets::texture_handle{};
        data.generic_textures[parameter.slot] = handle.is_valid() ? handle->index() : (white.is_valid() ? white->index() : 0u);
        break;
      }
    }
  }

  if (_has_material && _last_material == data) {
    return false;
  }

  _last_material = data;
  _has_material = true;

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  auto& buffer = registry.get<sbx::graphics::buffer>(_material_buffer);

  buffer.write(&_last_material, sizeof(shader_graph_preview_material_data));

  return true;
}

auto shader_graph_preview_renderer::_render() -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  const auto& target = registry.get<sbx::graphics::image>(_target);
  const auto& material_buffer = registry.get<sbx::graphics::buffer>(_material_buffer);
  const auto& mesh = *_sphere;

  const auto aspect = 1.0f; // square preview

  const auto view = sbx::math::matrix4x4::look_at(sbx::math::vector3{1.5f, 1.2f, 2.5f}, sbx::math::vector3{0.0f, 0.0f, 0.0f}, sbx::math::vector3{0.0f, 1.0f, 0.0f});
  const auto projection = sbx::math::matrix4x4::perspective(sbx::math::degree{45.0f}, aspect, 0.1f, 10.0f);

  auto push = shader_graph_preview_push_constants{};
  push.model_view_projection = projection * view;
  push.light_direction = sbx::math::vector4{-0.4f, -0.7f, -0.6f, static_cast<std::float_t>(_sampler_index)};
  push.light_color = sbx::math::vector4{1.0f, 1.0f, 1.0f, 3.0f};
  push.camera_position = sbx::math::vector4{1.5f, 1.2f, 2.5f, static_cast<std::float_t>(sbx::core::engine::time().value())};
  push.vertex_address = mesh.vertex_address();
  push.material_address = material_buffer.address();

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
  }

  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = target.view();
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.clearValue.color = VkClearColorValue{{0.11f, 0.11f, 0.15f, 1.0f}};

  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{_extent, _extent}};
  rendering_info.layerCount = 1u;
  rendering_info.colorAttachmentCount = 1u;
  rendering_info.pColorAttachments = &color_attachment;

  command_buffer.begin_rendering(rendering_info);

  const auto viewport = VkViewport{0.0f, 0.0f, static_cast<std::float_t>(_extent), static_cast<std::float_t>(_extent), 0.0f, 1.0f};
  command_buffer.set_viewport(viewport);

  const auto scissor = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{_extent, _extent}};
  command_buffer.set_scissor(scissor);

  command_buffer.bind_pipeline(*_pipeline);

  auto& index_buffer = registry.get<sbx::graphics::buffer>(mesh.index_buffer());
  command_buffer.bind_index_buffer(index_buffer, 0u, VK_INDEX_TYPE_UINT32);

  const auto& bindless_table = graphics_module.bindless_table();
  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  command_buffer.push_constants(bindless_table.pipeline_layout(), sbx::graphics::bindless_table::push_constant_stages, 0u, sbx::memory::as_bytes(push));

  const auto& submesh = mesh.submeshes().front();
  command_buffer.draw_indexed(submesh.index_count, 1u, submesh.index_offset, 0, 0u);

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

auto shader_graph_preview_renderer::update(const sbx::assets::shader_graph::create_info& graph) -> std::optional<ImTextureID> {
  _ensure_resources();

  auto changed = false;

  for (auto& result : _compiler.take_results(1u)) {
    if (!result.success) {
      _error = result.error;
      continue;
    }

    try {
      auto shader = std::make_unique<sbx::graphics::shader>(std::move(result.compiled), 0u);

      auto pipeline = std::make_unique<sbx::graphics::graphics_pipeline>(sbx::graphics::graphics_pipeline::create_info{
        .shader = sbx::memory::make_observer<const sbx::graphics::shader>(shader.get()),
        .color_formats = {sbx::graphics::format::r8g8b8a8_unorm},
        .depth_format = sbx::graphics::format::undefined,
        .cull_mode = sbx::graphics::cull_mode::back,
        .front_face = sbx::graphics::front_face::counter_clockwise,
        .depth_test = false,
        .depth_write = false,
        .samples = sbx::graphics::samples::count_1,
        .name = "Shader Graph Master Preview"
      });

      _shader = std::move(shader);
      _pipeline = std::move(pipeline);
      _error.clear();
      changed = true;
    } catch (const std::exception& exception) {
      _error = exception.what();
    }
  }

  _material_changed_last_update = _refresh_material(graph);
  changed |= _material_changed_last_update;

  if (!_shader || !_pipeline) {
    return std::nullopt;
  }

  if (!_sphere.is_valid() || _sphere->vertex_address() == 0u || _sphere->submeshes().empty()) {
    return std::nullopt;
  }

  if (changed || !_has_rendered_once) {
    _render();
    _has_rendered_once = true;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();
  const auto& target = registry.get<sbx::graphics::image>(_target);

  return ui_module.texture_id(target.view(), ui_module.thumbnail_sampler());
}

} // namespace editor
