// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/render_pass.hpp>

#include <filesystem>
#include <vector>

#include <fmt/format.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::render {

auto custom_shader_path(const assets::material& material) -> std::string {
  if (material.shading() == assets::shading_model::shader_graph && material.shader_graph().is_valid()) {
    return assets::shader_graph_generated_path(material.shader_graph()->id(), material.shader_graph()->generation());
  }

  if (material.shading() == assets::shading_model::shader_code) {
    return material.shader_code().generic_string();
  }

  return {};
}

auto resolve_custom_pipeline(const std::string& shader_path, std::span<const graphics::shader_compiler::entry_point_request> entry_points, graphics::graphics_pipeline::create_info pipeline_template, std::string_view pass_label) -> memory::observer_ptr<graphics::graphics_pipeline> {
  if (shader_path.empty()) {
    return {};
  }

  // A custom shader is only as good as whatever the user last wired up on the canvas or typed into
  // the file -- shader_compiler throws on a failed compile (missing/invalid entry point, Slang type
  // error, etc.), and this runs mid-frame inside a pass's own custom_pipeline_resolver callback with
  // nothing upstream catching it. A bad shader should skip that draw, not take the whole app down.
  try {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    auto& shader_cache = graphics_module.shader_cache();
    auto& pipeline_cache = graphics_module.pipeline_cache();

    const auto requests = std::vector<graphics::shader_compiler::entry_point_request>{entry_points.begin(), entry_points.end()};

    pipeline_template.shader = shader_cache.get({shader_path, requests});
    pipeline_template.name = fmt::format("{} Custom {}", pass_label, std::filesystem::path{shader_path}.stem().string());

    return pipeline_cache.get(pipeline_template);
  } catch (const std::exception& exception) {
    utility::logger<"render">::warn("'{}' failed to compile ({}) -- skipping {} for it until it's fixed", shader_path, exception.what(), pass_label);
    return {};
  }
}

// Shared per-command prologue for submit_draw_commands/_indirect: validity checks, the
// shader-graph-vs-fixed-slot pipeline resolution (see custom_pipeline_resolver's own doc comment),
// pipeline/mesh bind-state tracking (bound/current_pipeline/current_mesh persist across calls for
// the same command list, so a run of commands sharing a pipeline or mesh only rebinds once), and
// every push_constants field except transform_address and cascade_index -- the two fields the
// direct and indirect draw paths disagree on, left for the caller to fill in afterward. Returns
// false (skip this command entirely) for an invalid command, an out-of-range instance range, or a
// shader-graph material with no usable pipeline yet.
static auto prepare_draw_command(graphics::resource_registry& registry, render_context& context, const draw_command& command, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>& pipelines, const custom_pipeline_resolver& resolve_custom_pipeline, bool& bound, const graphics::graphics_pipeline*& current_pipeline, memory::observer_ptr<const assets::mesh>& current_mesh, push_constants& values) -> bool {
  if (!command.mesh.is_valid() || !command.material.is_valid() || !command.resident) {
    return false;
  }

  if (command.transform_offset + command.instance_count > context.instance_count) {
    return false;
  }

  auto pipeline = memory::observer_ptr<graphics::graphics_pipeline>{};

  if (command.material->shading() == assets::shading_model::shader_graph || command.material->shading() == assets::shading_model::shader_code) {
    const auto shader_path = custom_shader_path(*command.material);

    if (shader_path.empty() || !resolve_custom_pipeline) {
      return false;
    }

    pipeline = resolve_custom_pipeline(shader_path, command.material->is_double_sided());

    if (!pipeline) {
      return false; // no usable pipeline yet for this shader -- skip the draw rather than misrender
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

  values.frame_address = context.frame_address;
  values.vertex_address = command.vertex_address_override ? command.vertex_address_override : mesh.vertex_address();
  values.transform_offset = command.transform_offset;
  values.material_index = command.material->index();
  values.sampler_index = context.sampler_index;
  values.clamp_sampler_index = context.clamp_sampler_index;
  values.time = context.time;
  values.delta_time = context.delta_time;

  return true;
}

auto submit_draw_commands(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>& pipelines, std::uint32_t cascade_index, const custom_pipeline_resolver& resolve_custom_pipeline) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();

  auto bound = false;
  auto current_pipeline = static_cast<const graphics::graphics_pipeline*>(nullptr);
  auto current_mesh = memory::make_observer<const assets::mesh>(nullptr);

  for (const auto& command : commands) {
    auto values = push_constants{};
    values.cascade_index = cascade_index;

    if (!prepare_draw_command(registry, context, command, pipelines, resolve_custom_pipeline, bound, current_pipeline, current_mesh, values)) {
      continue;
    }

    values.transform_address = context.transform_address;

    context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, memory::as_bytes(values));

    const auto& submesh = command.mesh->submeshes()[command.submesh_index];
    context.command_buffer->draw_indexed(submesh.index_count, command.instance_count, submesh.index_offset, 0, 0u);
  }
}

auto submit_draw_commands_indirect(render_context& context, const std::vector<draw_command>& commands, const std::array<memory::observer_ptr<graphics::graphics_pipeline>, 4u>& pipelines, const custom_pipeline_resolver& resolve_custom_pipeline) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& indirect_args_buffer = registry.get<graphics::buffer>(context.culled_indirect_args_buffer);

  auto bound = false;
  auto current_pipeline = static_cast<const graphics::graphics_pipeline*>(nullptr);
  auto current_mesh = memory::make_observer<const assets::mesh>(nullptr);

  for (auto index = std::size_t{0u}; index < commands.size(); ++index) {
    const auto& command = commands[index];

    auto values = push_constants{};

    if (!prepare_draw_command(registry, context, command, pipelines, resolve_custom_pipeline, bound, current_pipeline, current_mesh, values)) {
      continue;
    }

    values.transform_address = context.culled_transform_address;

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
