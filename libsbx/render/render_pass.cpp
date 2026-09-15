// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/render_pass.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::render {

auto submit_draw_commands(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>& pipelines, std::uint32_t cascade_index, const graph_pipeline_resolver& resolve_graph_pipeline) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();

  auto bound = false;
  auto current_pipeline = static_cast<const graphics::graphics_pipeline*>(nullptr);
  auto current_mesh = memory::make_observer<const assets::mesh>(nullptr);

  for (const auto& command : commands) {
    if (!command.mesh.is_valid() || !command.material.is_valid() || !command.resident) {
      continue;
    }

    if (command.transform_offset + command.instance_count > context.instance_count) {
      continue;
    }

    // A material typed "Shader Graph" (material inspector's Material Type dropdown) bypasses
    // pipeline_id's fixed 4-slot table entirely -- there's no fixed slot for "however many
    // distinct graphs a scene uses" -- and defers to the pass's own resolver instead (every pass
    // supplies one; see graph_pipeline_resolver's doc comment). One with no graph assigned is
    // invalid by definition (material.hpp's shading_model doc comment) and skipped outright, same
    // as one whose graph exists but failed to compile.
    auto pipeline = memory::observer_ptr<graphics::graphics_pipeline>{};

    if (command.material->shading() == assets::shading_model::shader_graph) {
      if (!command.material->shader_graph().is_valid() || !resolve_graph_pipeline) {
        continue;
      }

      pipeline = resolve_graph_pipeline(command.material->shader_graph(), command.material->is_double_sided());

      if (!pipeline) {
        continue; // no usable pipeline yet for this graph -- skip the draw rather than misrender
      }
    } else {
      pipeline = pipelines[command.pipeline_id];
    }

    if (!bound || current_pipeline != pipeline.get()) {
      context.command_buffer->bind_pipeline(*pipeline);
      current_pipeline = pipeline.get();
      bound = true;
    }

    const auto& mesh = *command.mesh;

    if (current_mesh.get() != &mesh) {
      auto& index_buffer = registry.get<graphics::buffer>(mesh.index_buffer());
      context.command_buffer->bind_index_buffer(index_buffer, 0u, VK_INDEX_TYPE_UINT32);
      current_mesh = &mesh;
    }

    const auto& submesh = mesh.submeshes()[command.submesh_index];

    auto values = push_constants{};
    values.frame_address = context.frame_address;
    values.vertex_address = command.vertex_address_override ? command.vertex_address_override : mesh.vertex_address();
    values.transform_address = context.transform_address;
    values.transform_offset = command.transform_offset;
    values.material_index = command.material->index();
    values.sampler_index = context.sampler_index;
    values.clamp_sampler_index = context.clamp_sampler_index;
    values.cascade_index = cascade_index;

    context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, memory::as_bytes(values));

    context.command_buffer->draw_indexed(submesh.index_count, command.instance_count, submesh.index_offset, 0, 0u);
  }
}

auto submit_draw_commands_indirect(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>& pipelines, const graph_pipeline_resolver& resolve_graph_pipeline) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& indirect_args_buffer = registry.get<graphics::buffer>(context.culled_indirect_args_buffer);

  auto bound = false;
  auto current_pipeline = static_cast<const graphics::graphics_pipeline*>(nullptr);
  auto current_mesh = memory::make_observer<const assets::mesh>(nullptr);

  for (auto index = std::size_t{0u}; index < commands.size(); ++index) {
    const auto& command = commands[index];

    if (!command.mesh.is_valid() || !command.material.is_valid() || !command.resident) {
      continue;
    }

    if (command.transform_offset + command.instance_count > context.instance_count) {
      continue;
    }

    auto pipeline = memory::observer_ptr<graphics::graphics_pipeline>{};

    if (command.material->shading() == assets::shading_model::shader_graph) {
      if (!command.material->shader_graph().is_valid() || !resolve_graph_pipeline) {
        continue;
      }

      pipeline = resolve_graph_pipeline(command.material->shader_graph(), command.material->is_double_sided());

      if (!pipeline) {
        continue; // no usable pipeline yet for this graph -- skip the draw rather than misrender
      }
    } else {
      pipeline = pipelines[command.pipeline_id];
    }

    if (!bound || current_pipeline != pipeline.get()) {
      context.command_buffer->bind_pipeline(*pipeline);
      current_pipeline = pipeline.get();
      bound = true;
    }

    const auto& mesh = *command.mesh;

    if (current_mesh.get() != &mesh) {
      auto& index_buffer = registry.get<graphics::buffer>(mesh.index_buffer());
      context.command_buffer->bind_index_buffer(index_buffer, 0u, VK_INDEX_TYPE_UINT32);
      current_mesh = &mesh;
    }

    auto values = push_constants{};
    values.frame_address = context.frame_address;
    values.vertex_address = command.vertex_address_override ? command.vertex_address_override : mesh.vertex_address();
    values.transform_address = context.culled_transform_address;
    values.transform_offset = command.transform_offset;
    values.material_index = command.material->index();
    values.sampler_index = context.sampler_index;
    values.clamp_sampler_index = context.clamp_sampler_index;

    context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, memory::as_bytes(values));

    const auto offset = context.culled_indirect_args_slot_offset + static_cast<std::uint32_t>(index);
    context.command_buffer->draw_indexed_indirect(indirect_args_buffer, offset, 1u);
  }
}

auto bind_globals(render_context& context) -> void {
  bind_globals(context, context.extent);
}

auto bind_globals(render_context& context, const math::vector2u& extent) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(*context.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  const auto viewport = VkViewport{0.0f, 0.0f, static_cast<std::float_t>(extent.x()), static_cast<std::float_t>(extent.y()), 0.0f, 1.0f};
  context.command_buffer->set_viewport(viewport);

  const auto scissor = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{extent.x(), extent.y()}};
  context.command_buffer->set_scissor(scissor);
}

auto bind_compute_globals(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  const auto descriptor_set = bindless_table.descriptor_set();

  vkCmdBindDescriptorSets(*context.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);
}

} // namespace sbx::render
