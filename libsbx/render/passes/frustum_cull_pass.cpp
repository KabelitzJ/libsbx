// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/frustum_cull_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

#include <algorithm>
#include <array>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/math/vector4.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

namespace sbx::render {

inline constexpr auto threads_per_group = std::uint32_t{64u};

// Mirror frustum_cull_common.slang's occlusion_mode_*.
inline constexpr auto occlusion_mode_none = std::uint32_t{0u};
inline constexpr auto occlusion_mode_early = std::uint32_t{1u};
inline constexpr auto occlusion_mode_late = std::uint32_t{2u};

frustum_cull_pass::frustum_cull_pass(bool late)
: _late{late} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto shader = shader_cache.get({"engine://shaders/passes/frustum_cull.slang", entry_points});

  _pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = shader,
    .name = "Frustum Cull"
  });

  _instanced_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = shader_cache.get({"engine://shaders/passes/frustum_cull_instanced.slang", entry_points}),
    .name = "Frustum Cull Instanced"
  });
}

// Mirrors frustum_cull.slang's push_data. dest_transforms and indirect_args point at the command's level 0; level k follows at + k * instance_count transforms and + k args.
struct frustum_cull_push_data {
  graphics::buffer::address_type frame_address;
  graphics::buffer::address_type source_transforms;
  graphics::buffer::address_type dest_transforms;
  graphics::buffer::address_type indirect_args;
  math::vector4 local_bounds_min; // .w unused
  math::vector4 local_bounds_max; // .w unused
  std::array<std::float_t, 4u> lod_errors;
  std::uint32_t transform_offset;
  std::uint32_t instance_count;
  std::uint32_t cascade_index;
  std::uint32_t lod_count;
  std::uint32_t occlusion_offset;
  std::uint32_t occlusion_mode;
}; // struct frustum_cull_push_data

// Mirrors frustum_cull_instanced.slang's push_data; the padding puts the float4s at offset 48, since math::vector4 is only 4-byte aligned.
struct frustum_cull_instanced_push_data {
  graphics::buffer::address_type frame_address;
  graphics::buffer::address_type node_transforms;
  graphics::buffer::address_type instances;
  graphics::buffer::address_type dest_transforms;
  graphics::buffer::address_type indirect_args;
  std::uint64_t padding{0u};
  math::vector4 local_bounds_min; // .w unused
  math::vector4 local_bounds_max; // .w unused
  std::array<std::float_t, 4u> lod_errors;
  std::uint32_t node_index;
  std::uint32_t instance_count;
  std::uint32_t cascade_index;
  std::uint32_t lod_count;
  std::uint32_t occlusion_offset;
  std::uint32_t occlusion_mode;
}; // struct frustum_cull_instanced_push_data

static_assert(sizeof(frustum_cull_push_data) <= 128u && sizeof(frustum_cull_instanced_push_data) <= 128u, "Cull push constants must fit the shared 128-byte push-constant range.");

auto frustum_cull_pass::declare(compute_pass_builder& builder, const graph_resources& resources) -> void {
  // instanceCount is bumped atomically, hence read|write; the consumers' declared reads place the barrier.
  builder.writes_buffer(resources.culled_indirect_args_buffer, graphics::pipeline_stage::compute_shader, graphics::access::shader_read | graphics::access::shader_write);
  builder.writes_buffer(resources.culled_transform_buffer, graphics::pipeline_stage::compute_shader, graphics::access::shader_write);
  builder.writes_buffer(resources.instanced_culled_buffer, graphics::pipeline_stage::compute_shader, graphics::access::shader_write);

  // The visibility flags persist across frames: the early cull reads what the previous frame's late cull wrote.
  if (_late) {
    builder.reads_image(resources.hiz, graphics::pipeline_stage::compute_shader, graphics::access::shader_sampled_read, graphics::image_layout::general);
    builder.writes_buffer(resources.occlusion_visibility_buffer, graphics::pipeline_stage::compute_shader, graphics::access::shader_read | graphics::access::shader_write);
  } else {
    builder.reads_buffer(resources.occlusion_visibility_buffer, graphics::pipeline_stage::compute_shader, graphics::access::shader_read);
  }
}

auto frustum_cull_pass::execute(render_context& context) -> void {
  SBX_PROFILE_SCOPE("frustum_cull_pass::execute");
  SBX_STATS_SCOPE("frustum_cull_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "frustum_cull_pass::execute");

  if (!context.packet->camera.is_active) {
    return;
  }

  bind_compute_globals(context);

  if (_late) {
    _cull_view(context, context.packet->opaque_commands, late_camera_view, occlusion_mode_late);
    return;
  }

  _cull_view(context, context.packet->opaque_commands, 0xFFFFFFFFu, context.occlusion_enabled ? occlusion_mode_early : occlusion_mode_none);

  if (context.has_shadow_caster) {
    for (auto cascade = std::uint32_t{0u}; cascade < shadow_cascade_count; ++cascade) {
      _cull_view(context, context.packet->shadow_caster_commands, cascade, occlusion_mode_none);
    }
  }

  // Local light tiles are shadow views after the cascades, culled whether or not the sun casts.
  const auto local_view_count = static_cast<std::uint32_t>(context.packet->local_shadow_views.size());

  for (auto tile = std::uint32_t{0u}; tile < local_view_count; ++tile) {
    _cull_view(context, context.packet->shadow_caster_commands, shadow_cascade_count + tile, occlusion_mode_none);
  }
}

auto frustum_cull_pass::_cull_view(render_context& context, const std::vector<draw_command>& commands, std::uint32_t cascade_index, std::uint32_t occlusion_mode) -> void {
  const auto view = cull_view_index(context, cascade_index);
  // The late view culls against the camera frustum, as the camera.
  const auto frustum = cascade_index == late_camera_view ? 0xFFFFFFFFu : cascade_index;
  const auto dest_transforms = context.culled_transform_address + view * context.culled_transform_view_stride * sizeof(transform_data);
  const auto indirect_args = context.culled_indirect_args_address + view * context.culled_indirect_args_view_stride * sizeof(VkDrawIndexedIndirectCommand);

  // Commands of both kinds interleave, so only rebind on change.
  auto bound = memory::observer_ptr<graphics::compute_pipeline>{};

  for (const auto& command : commands) {
    // Same skip conditions as submit_draw_commands_indirect.
    if (!command.mesh.is_valid() || !command.material.is_valid() || !command.resident || command.instance_count == 0u || command.lod_args_offset + command.lod_count > context.culled_indirect_args_view_stride) {
      continue;
    }

    const auto& bounds = command.local_bounds;
    const auto groups = (command.instance_count + threads_per_group - 1u) / threads_per_group;
    const auto command_args = indirect_args + command.lod_args_offset * sizeof(VkDrawIndexedIndirectCommand);

    if (command.instances) {
      if (bound != _instanced_pipeline) {
        context.command_buffer->bind_pipeline(*_instanced_pipeline);
        bound = _instanced_pipeline;
      }

      const auto push = frustum_cull_instanced_push_data{
        context.frame_address,
        context.transform_address,
        core::engine::get_module<graphics::graphics_module>().resource_registry().get<graphics::buffer>(command.instances->buffer()).address(),
        instanced_transform_address(context, command, cascade_index, 0u),
        command_args,
        0u,
        math::vector4{bounds.min(), 0.0f},
        math::vector4{bounds.max(), 0.0f},
        command.lod_errors,
        command.transform_offset,
        command.instance_count,
        frustum,
        command.lod_count,
        command.occlusion_offset,
        occlusion_mode
      };

      write_push_constants(context, push);
      context.command_buffer->dispatch(groups, 1u, 1u);
      continue;
    }

    if (command.transform_offset + command.instance_count > context.instance_count) {
      continue;
    }

    if (bound != _pipeline) {
      context.command_buffer->bind_pipeline(*_pipeline);
      bound = _pipeline;
    }

    const auto push = frustum_cull_push_data{
      context.frame_address,
      context.transform_address,
      dest_transforms + static_cast<graphics::buffer::address_type>(command.lod_culled_offset) * sizeof(transform_data),
      command_args,
      math::vector4{bounds.min(), 0.0f},
      math::vector4{bounds.max(), 0.0f},
      command.lod_errors,
      command.transform_offset,
      command.instance_count,
      frustum,
      command.lod_count,
      command.occlusion_offset,
      occlusion_mode
    };

    write_push_constants(context, push);
    context.command_buffer->dispatch(groups, 1u, 1u);
  }
}

} // namespace sbx::render
