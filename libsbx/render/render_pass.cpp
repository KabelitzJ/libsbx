// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/render_pass.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/assets/assets_module.hpp>

namespace sbx::render {

auto submit_draw_commands(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 2u>& pipelines) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();

  auto bound = false;
  auto current_pipeline = std::uint32_t{0u};
  const auto* current_mesh = static_cast<const assets::mesh*>(nullptr);

  for (const auto& command : commands) {
    if (!command.mesh.is_valid() || !assets_module.is_resident(command.mesh)) {
      continue;
    }

    if (!command.material.is_valid() || !assets_module.is_resident(command.material)) {
      continue;
    }

    if (command.transform_offset + command.instance_count > context.instance_count) {
      continue;
    }

    if (!bound || current_pipeline != command.pipeline_id) {
      context.command_buffer->bind_pipeline(*pipelines[command.pipeline_id]);
      current_pipeline = command.pipeline_id;
      bound = true;
    }

    const auto& mesh = *command.mesh;

    if (current_mesh != &mesh) {
      vkCmdBindIndexBuffer(*context.command_buffer, registry.get<graphics::buffer>(mesh.index_buffer()).handle(), 0u, VK_INDEX_TYPE_UINT32);
      current_mesh = &mesh;
    }

    const auto& submesh = mesh.submeshes()[command.submesh_index];

    auto values = push_constants{};
    values.frame_address = context.frame_address;
    values.vertex_address = mesh.vertex_address();
    values.transform_address = context.transform_address;
    values.transform_offset = command.transform_offset;
    values.material_index = command.material->index();
    values.sampler_index = context.sampler_index;

    auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
    std::memcpy(range.data(), &values, sizeof(push_constants));

    vkCmdPushConstants(*context.command_buffer, bindless_table.pipeline_layout(), VK_SHADER_STAGE_ALL, 0u, static_cast<std::uint32_t>(range.size()), range.data());

    vkCmdDrawIndexed(*context.command_buffer, submesh.index_count, command.instance_count, submesh.index_offset, 0, 0u);
  }
}

auto bind_globals(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(*context.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  const auto viewport = VkViewport{0.0f, 0.0f, static_cast<std::float_t>(context.extent.x()), static_cast<std::float_t>(context.extent.y()), 0.0f, 1.0f};
  context.command_buffer->set_viewport(viewport);

  const auto scissor = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{context.extent.x(), context.extent.y()}};
  context.command_buffer->set_scissor(scissor);
}

} // namespace sbx::render
