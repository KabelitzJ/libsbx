// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/panels/shader_graph_node_preview_manager.hpp>

#include <cmath>
#include <unordered_set>

#include <vulkan/vulkan.h>

#include <fmt/format.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/resource_registry.hpp>
#include <libsbx/graphics/bindless_table.hpp>

#include <libsbx/memory/observer_ptr.hpp>
#include <libsbx/memory/bytes.hpp>

#include <libsbx/render/ui/ui_module.hpp>

#include <libsbx/assets/shader_graph_codegen.hpp>

#include <editor/panels/shader_graph_preview.hpp>

namespace editor {

shader_graph_node_preview_manager::~shader_graph_node_preview_manager() {
  clear();
}

auto shader_graph_node_preview_manager::clear() -> void {
  if (_entries.empty()) {
    return;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  auto& frame_context = graphics_module.frame_context();
  const auto frame_index = frame_context.frame_index();

  for (auto& [node_id, state] : _entries) {
    if (state.resources_ready) {
      registry.retire(state.target, frame_index);
    }
  }

  _entries.clear();
}

auto shader_graph_node_preview_manager::request_recompile(const sbx::assets::shader_graph::create_info& graph, std::uint32_t node_id) -> void {
  auto& state = _entries[node_id]; // creates tracking state if this node wasn't already previewed

  const auto source = sbx::assets::generate_node_preview_source(fmt::format("shader_graph_node_preview_{}", node_id), graph, node_id, 0u);

  if (!source) {
    state.error = source.error();
    return;
  }

  state.error.clear();

  auto request = sbx::graphics::async_shader_compiler::request{};
  request.key = node_id;
  request.path = sbx::filesystem::engine_data_directory() / "shaders" / "generated" / "preview" / fmt::format("node_preview_{}.slang", node_id);
  request.source = *source;
  request.entry_points = std::vector<sbx::graphics::shader_compiler::entry_point_request>{
    sbx::graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    sbx::graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  _compiler.submit(std::move(request));
}

auto shader_graph_node_preview_manager::forget(std::uint32_t node_id) -> void {
  const auto entry = _entries.find(node_id);

  if (entry == _entries.end()) {
    return;
  }

  if (entry->second.resources_ready) {
    auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
    auto& registry = graphics_module.resource_registry();
    auto& frame_context = graphics_module.frame_context();
    const auto frame_index = frame_context.frame_index();

    registry.retire(entry->second.target, frame_index);
  }

  _entries.erase(entry);
}

auto shader_graph_node_preview_manager::_ensure_target(entry& state, std::uint32_t node_id) -> void {
  if (state.resources_ready) {
    return;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  state.target = registry.emplace<sbx::graphics::image>(sbx::graphics::image::create_info{
    .extent = {_extent, _extent, 1u},
    .format = sbx::graphics::format::r8g8b8a8_unorm,
    .usage = sbx::graphics::image_usage::color_attachment | sbx::graphics::image_usage::sampled,
    .name = fmt::format("Shader Graph Node Preview {}", node_id)
  });

  state.resources_ready = true;
}

auto shader_graph_node_preview_manager::_render(entry& state, sbx::graphics::buffer::address_type material_address, std::uint32_t sampler_index) -> void {
  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  const auto& target = registry.get<sbx::graphics::image>(state.target);

  auto push = shader_graph_node_preview_push_constants{};
  push.material_address = material_address;
  push.sampler_index = static_cast<std::float_t>(sampler_index);
  push.time = static_cast<std::float_t>(sbx::core::engine::time().value());
  push.delta_time = static_cast<std::float_t>(sbx::core::engine::delta_time().value());

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
  color_attachment.clearValue.color = VkClearColorValue{{0.0f, 0.0f, 0.0f, 1.0f}};

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

  command_buffer.bind_pipeline(*state.pipeline);

  const auto& bindless_table = graphics_module.bindless_table();
  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  command_buffer.push_constants(bindless_table.pipeline_layout(), sbx::graphics::bindless_table::push_constant_stages, 0u, sbx::memory::as_bytes(push));

  // Fullscreen triangle -- no vertex/index buffer at all, see generate_node_preview_source's own
  // doc comment for the SV_VertexID trick this pairs with.
  command_buffer.draw(3u, 1u, 0u, 0u);

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

auto shader_graph_node_preview_manager::update(sbx::graphics::buffer::address_type material_address, std::uint32_t sampler_index, bool material_changed) -> void {
  auto refreshed = std::unordered_set<std::uint32_t>{};

  for (auto& result : _compiler.take_results(8u)) {
    const auto node_id = static_cast<std::uint32_t>(result.key);

    // Not operator[] -- a result landing after forget() erased this node's entry means nobody
    // will ever call texture_id(node_id) again; discard rather than resurrect a tracking entry
    // for it (same reasoning as asset_loader's own "discard if nobody will ever drain it").
    const auto entry = _entries.find(node_id);

    if (entry == _entries.end()) {
      continue;
    }

    auto& state = entry->second;

    if (!result.success) {
      state.error = result.error;
      continue;
    }

    try {
      auto shader = std::make_unique<sbx::graphics::shader>(std::move(result.compiled), 0u);

      auto pipeline = std::make_unique<sbx::graphics::graphics_pipeline>(sbx::graphics::graphics_pipeline::create_info{
        .shader = sbx::memory::make_observer<const sbx::graphics::shader>(shader.get()),
        .color_formats = {sbx::graphics::format::r8g8b8a8_unorm},
        .depth_format = sbx::graphics::format::undefined,
        .cull_mode = sbx::graphics::cull_mode::none,
        .front_face = sbx::graphics::front_face::counter_clockwise,
        .depth_test = false,
        .depth_write = false,
        .samples = sbx::graphics::samples::count_1,
        .name = fmt::format("Shader Graph Node Preview {}", node_id)
      });

      state.shader = std::move(shader);
      state.pipeline = std::move(pipeline);
      state.error.clear();
      refreshed.insert(node_id);
    } catch (const std::exception& exception) {
      state.error = exception.what();
    }
  }

  for (auto& [node_id, state] : _entries) {
    if (!state.shader || !state.pipeline) {
      continue;
    }

    _ensure_target(state, node_id);

    if (refreshed.contains(node_id) || material_changed || !state.has_rendered_once) {
      _render(state, material_address, sampler_index);
      state.has_rendered_once = true;
    }
  }
}

auto shader_graph_node_preview_manager::texture_id(std::uint32_t node_id) const -> std::optional<ImTextureID> {
  const auto entry = _entries.find(node_id);

  if (entry == _entries.end() || !entry->second.has_rendered_once) {
    return std::nullopt;
  }

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  auto& ui_module = sbx::core::engine::get_module<sbx::render::ui_module>();
  const auto& target = registry.get<sbx::graphics::image>(entry->second.target);

  return ui_module.texture_id(target.view(), ui_module.thumbnail_sampler());
}

auto shader_graph_node_preview_manager::error(std::uint32_t node_id) const -> std::string {
  const auto entry = _entries.find(node_id);
  return entry == _entries.end() ? std::string{} : entry->second.error;
}

} // namespace editor
