// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/particle_pass.hpp>

#include <algorithm>
#include <cstring>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/vector4.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

// New buffers are sized at this multiple of what's actually needed, same reasoning as debug_draw_pass.
constexpr auto growth_factor = 1.5f;

inline constexpr auto alpha_blend_group = std::uint32_t{0u};
inline constexpr auto additive_group = std::uint32_t{1u};

struct particle_billboard_push {
  graphics::buffer::address_type frame_address;
  graphics::buffer::address_type instance_address;
  std::uint32_t instance_offset;
  std::uint32_t sampler_index;
  // Explicit padding rather than relying on an implicit rule -- both this struct and the shader's
  // matching push_data lay every field out by hand so a float4's usual 16-byte hardware alignment
  // can never silently disagree between the two sides.
  std::uint32_t _padding0;
  std::uint32_t _padding1;
  math::vector4 camera_right;
  math::vector4 camera_up;
}; // struct particle_billboard_push

struct particle_mesh_push {
  graphics::buffer::address_type frame_address;
  graphics::buffer::address_type vertex_address;
  graphics::buffer::address_type instance_address;
  std::uint32_t instance_offset;
  std::uint32_t material_index;
  std::uint32_t sampler_index;
}; // struct particle_mesh_push

namespace {

// Shared by both the billboard and mesh pipelines: group 0's weighted-OIT pair for alpha_blend, and
// group 1's single-attachment true-additive blend (see particle_pass's doc comment for why additive
// bypasses OIT). Only the shader differs between billboard and mesh variants.
auto make_alpha_blend_blend_attachments() -> std::vector<graphics::blend_attachment> {
  return {
    graphics::blend_attachment{
      .enable = true,
      .source_color = graphics::blend_factor::one,
      .destination_color = graphics::blend_factor::one,
      .color_operation = graphics::blend_operation::add,
      .source_alpha = graphics::blend_factor::one,
      .destination_alpha = graphics::blend_factor::one,
      .alpha_operation = graphics::blend_operation::add
    },
    graphics::blend_attachment{
      .enable = true,
      .source_color = graphics::blend_factor::zero,
      .destination_color = graphics::blend_factor::one_minus_source_color,
      .color_operation = graphics::blend_operation::add,
      .source_alpha = graphics::blend_factor::zero,
      .destination_alpha = graphics::blend_factor::one_minus_source_color,
      .alpha_operation = graphics::blend_operation::add
    }
  };
}

auto make_additive_blend_attachments() -> std::vector<graphics::blend_attachment> {
  return {
    graphics::blend_attachment{
      .enable = true,
      .source_color = graphics::blend_factor::one,
      .destination_color = graphics::blend_factor::one,
      .color_operation = graphics::blend_operation::add,
      .source_alpha = graphics::blend_factor::one,
      .destination_alpha = graphics::blend_factor::one,
      .alpha_operation = graphics::blend_operation::add
    }
  };
}

} // namespace

particle_pass::particle_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto alpha_blend_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main_alpha_blend"}
  };

  const auto additive_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main_additive"}
  };

  const auto& billboard_alpha_blend_shader = shader_cache.get({"shaders/particles/particle_billboard.slang", alpha_blend_entry_points});
  const auto& billboard_additive_shader = shader_cache.get({"shaders/particles/particle_billboard.slang", additive_entry_points});
  const auto& mesh_alpha_blend_shader = shader_cache.get({"shaders/particles/particle_mesh.slang", alpha_blend_entry_points});
  const auto& mesh_additive_shader = shader_cache.get({"shaders/particles/particle_mesh.slang", additive_entry_points});

  // Group 0 pipeline: two color attachments (accumulator + revealage), the weighted-OIT pair,
  // identical to transparent_accumulate_pass's blend state.
  auto billboard_alpha_blend_info = graphics::graphics_pipeline::create_info{
    .shader = billboard_alpha_blend_shader,
    .color_formats = {render_pass::hdr_format, graphics::format::r16_sfloat},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = graphics::cull_mode::none,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .color_blend_attachments = make_alpha_blend_blend_attachments(),
    .name = "Particles Billboard Alpha Blend"
  };

  // Group 1 pipeline: one color attachment (the real scene color target), a plain (one, one, add)
  // additive blend -- true additive, not routed through the OIT weighting at all (see this class's
  // doc comment for why that would be wrong).
  auto billboard_additive_info = graphics::graphics_pipeline::create_info{
    .shader = billboard_additive_shader,
    .color_formats = {render_pass::hdr_format},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = graphics::cull_mode::none,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .color_blend_attachments = make_additive_blend_attachments(),
    .name = "Particles Billboard Additive"
  };

  // Mesh particle pipelines mirror the billboard ones exactly (same two groups, same blend states) --
  // cull_mode::none since mesh particles rarely benefit from backface culling (small, fast-moving,
  // often not authored with outward-facing normals in mind).
  auto mesh_alpha_blend_info = billboard_alpha_blend_info;
  mesh_alpha_blend_info.shader = mesh_alpha_blend_shader;
  mesh_alpha_blend_info.name = "Particles Mesh Alpha Blend";

  auto mesh_additive_info = billboard_additive_info;
  mesh_additive_info.shader = mesh_additive_shader;
  mesh_additive_info.name = "Particles Mesh Additive";

  _billboard_pipelines[alpha_blend_group] = pipeline_cache.get(billboard_alpha_blend_info);
  _billboard_pipelines[additive_group] = pipeline_cache.get(billboard_additive_info);
  _mesh_pipelines[alpha_blend_group] = pipeline_cache.get(mesh_alpha_blend_info);
  _mesh_pipelines[additive_group] = pipeline_cache.get(mesh_additive_info);
}

auto particle_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  auto oit_group = render_attachment_group{.extent = resources.extent};

  oit_group.colors.push_back(color_attachment_slot{
    .image = resources.accumulator_msaa,
    .access_mask = graphics::access::color_attachment_write | graphics::access::color_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .resolve_image = resources.accumulator
  });

  oit_group.colors.push_back(color_attachment_slot{
    .image = resources.revealage_msaa,
    .access_mask = graphics::access::color_attachment_write | graphics::access::color_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .resolve_image = resources.revealage
  });

  oit_group.depth = depth_attachment_slot{.image = resources.depth};

  builder.add_group(oit_group);

  auto additive_group_attachments = render_attachment_group{.extent = resources.extent};

  additive_group_attachments.colors.push_back(color_attachment_slot{
    .image = resources.color_msaa,
    .access_mask = graphics::access::color_attachment_write | graphics::access::color_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .resolve_image = resources.color
  });

  additive_group_attachments.depth = depth_attachment_slot{.image = resources.depth};

  builder.add_group(additive_group_attachments);
}

auto particle_pass::should_execute(const render_context& context, std::uint32_t group) const -> bool {
  if (!context.packet->camera.is_active) {
    return false;
  }

  const auto target_mode = group == alpha_blend_group ? assets::emitter_blend_mode::alpha_blend : assets::emitter_blend_mode::additive;

  const auto has_billboards = std::ranges::any_of(context.packet->particle_billboard_commands, [target_mode](const auto& command) {
    return command.blend_mode == target_mode;
  });

  const auto has_meshes = std::ranges::any_of(context.packet->particle_mesh_commands, [target_mode](const auto& command) {
    return command.blend_mode == target_mode;
  });

  return has_billboards || has_meshes;
}

auto particle_pass::_ensure_uploaded(render_context& context) -> void {
  if (_uploaded_frame == context.frame_index) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  const auto slot = context.slot;

  const auto& billboard_instances = context.packet->particle_billboard_instances;

  if (!_billboard_buffers[slot].is_valid() || billboard_instances.size() > _billboard_capacities[slot]) {
    if (_billboard_buffers[slot].is_valid()) {
      registry.retire<graphics::buffer>(_billboard_buffers[slot], context.frame_index);
    }

    const auto new_capacity = static_cast<std::size_t>(static_cast<std::float_t>(billboard_instances.size()) * growth_factor) + 1u;

    _billboard_buffers[slot] = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = static_cast<graphics::buffer::size_type>(new_capacity * sizeof(particle_billboard_instance)),
      .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
      .memory = graphics::memory_usage::host_write,
      .name = "Particle Billboard Instances"
    });

    _billboard_capacities[slot] = new_capacity;
  }

  registry.get<graphics::buffer>(_billboard_buffers[slot]).write(std::span{billboard_instances});

  const auto& mesh_instances = context.packet->particle_mesh_instances;

  if (!_mesh_buffers[slot].is_valid() || mesh_instances.size() > _mesh_capacities[slot]) {
    if (_mesh_buffers[slot].is_valid()) {
      registry.retire<graphics::buffer>(_mesh_buffers[slot], context.frame_index);
    }

    const auto new_capacity = static_cast<std::size_t>(static_cast<std::float_t>(mesh_instances.size()) * growth_factor) + 1u;

    _mesh_buffers[slot] = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = static_cast<graphics::buffer::size_type>(new_capacity * sizeof(particle_mesh_instance)),
      .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
      .memory = graphics::memory_usage::host_write,
      .name = "Particle Mesh Instances"
    });

    _mesh_capacities[slot] = new_capacity;
  }

  registry.get<graphics::buffer>(_mesh_buffers[slot]).write(std::span{mesh_instances});

  _uploaded_frame = context.frame_index;
}

auto particle_pass::_draw_billboards(render_context& context, std::uint32_t group) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();
  auto& registry = graphics_module.resource_registry();

  const auto& buffer = registry.get<graphics::buffer>(_billboard_buffers[context.slot]);

  // The camera's world-space right/up axes, for orienting each billboard toward it -- derived from
  // the camera's own world matrix (the inverse of the view matrix already in the packet) rather than
  // reading rows out of the view matrix directly, to sidestep any row/column convention ambiguity.
  const auto camera_world = math::matrix4x4::inverted(context.packet->camera.view);
  const auto camera_right = math::vector3{camera_world[0]};
  const auto camera_up = math::vector3{camera_world[1]};

  context.command_buffer->bind_pipeline(*_billboard_pipelines[group]);

  const auto target_mode = group == alpha_blend_group ? assets::emitter_blend_mode::alpha_blend : assets::emitter_blend_mode::additive;

  for (const auto& command : context.packet->particle_billboard_commands) {
    if (command.blend_mode != target_mode) {
      continue;
    }

    auto values = particle_billboard_push{
      context.frame_address,
      buffer.address(),
      command.instance_offset,
      context.sampler_index,
      0u,
      0u,
      math::vector4{camera_right, 0.0f},
      math::vector4{camera_up, 0.0f}
    };

    auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
    std::memcpy(range.data(), &values, sizeof(values));

    context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

    context.command_buffer->draw(6u, command.instance_count, 0u, 0u);
  }
}

auto particle_pass::_draw_meshes(render_context& context, std::uint32_t group) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();
  auto& registry = graphics_module.resource_registry();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto& buffer = registry.get<graphics::buffer>(_mesh_buffers[context.slot]);

  const auto target_mode = group == alpha_blend_group ? assets::emitter_blend_mode::alpha_blend : assets::emitter_blend_mode::additive;

  auto bound = false;
  const auto* current_mesh = static_cast<const assets::mesh*>(nullptr);

  for (const auto& command : context.packet->particle_mesh_commands) {
    if (command.blend_mode != target_mode) {
      continue;
    }

    if (!command.mesh.is_valid() || !assets_module.is_resident(command.mesh) || !command.material.is_valid() || !assets_module.is_resident(command.material)) {
      continue;
    }

    if (!bound) {
      context.command_buffer->bind_pipeline(*_mesh_pipelines[group]);
      bound = true;
    }

    const auto& mesh = *command.mesh;

    if (current_mesh != &mesh) {
      auto& index_buffer = registry.get<graphics::buffer>(mesh.index_buffer());
      context.command_buffer->bind_index_buffer(index_buffer, 0u, VK_INDEX_TYPE_UINT32);
      current_mesh = &mesh;
    }

    const auto& submesh = mesh.submeshes()[command.submesh_index];

    auto values = particle_mesh_push{
      context.frame_address,
      mesh.vertex_address(),
      buffer.address(),
      command.instance_offset,
      command.material->index(),
      context.sampler_index
    };

    auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
    std::memcpy(range.data(), &values, sizeof(values));

    context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

    context.command_buffer->draw_indexed(submesh.index_count, command.instance_count, submesh.index_offset, 0, 0u);
  }
}

auto particle_pass::execute(render_context& context, std::uint32_t group) -> void {
  _ensure_uploaded(context);

  bind_globals(context);

  _draw_billboards(context, group);
  _draw_meshes(context, group);
}

} // namespace sbx::render
