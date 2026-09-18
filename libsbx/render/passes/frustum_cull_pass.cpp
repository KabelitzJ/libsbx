// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/frustum_cull_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

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

frustum_cull_pass::frustum_cull_pass() {
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
}

struct frustum_cull_push_data {
  graphics::buffer::address_type frame_address;
  graphics::buffer::address_type source_transforms;
  graphics::buffer::address_type dest_transforms;
  graphics::buffer::address_type indirect_args;
  math::vector4 local_bounds_min; // .w unused
  math::vector4 local_bounds_max; // .w unused
  std::uint32_t command_index;
  std::uint32_t transform_offset;
  std::uint32_t instance_count;
  std::uint32_t padding{0u};
}; // struct frustum_cull_push_data

auto frustum_cull_pass::declare(compute_pass_builder& builder, const graph_resources& resources) -> void {
  builder.declares_buffer_ready(resources.culled_indirect_args_buffer, graphics::pipeline_stage::draw_indirect, graphics::access::indirect_command_read);
  builder.declares_buffer_ready(resources.culled_transform_buffer, graphics::pipeline_stage::vertex_shader, graphics::access::shader_read);
}

auto frustum_cull_pass::execute(render_context& context) -> void {
  SBX_PROFILE_SCOPE("frustum_cull_pass::execute");
  SBX_STATS_SCOPE("frustum_cull_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "frustum_cull_pass::execute");

  if (!context.packet->camera.is_active) {
    return;
  }

  const auto& commands = context.packet->opaque_commands;

  if (commands.empty()) {
    return;
  }

  bind_compute_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  for (auto index = std::uint32_t{0u}; index < static_cast<std::uint32_t>(commands.size()); ++index) {
    const auto& command = commands[index];

    // Same skip condition submit_draw_commands_indirect itself uses -- no point culling instances
    // for a command that won't be drawn either way.
    if (!command.mesh.is_valid() || !command.material.is_valid() || !command.resident || command.instance_count == 0u) {
      continue;
    }

    if (command.transform_offset + command.instance_count > context.instance_count) {
      continue;
    }

    const auto& bounds = command.local_bounds;

    const auto push = frustum_cull_push_data{
      context.frame_address,
      context.transform_address,
      context.culled_transform_address,
      context.culled_indirect_args_address,
      math::vector4{bounds.min(), 0.0f},
      math::vector4{bounds.max(), 0.0f},
      index,
      command.transform_offset,
      command.instance_count
    };

    write_push_constants(context, push);

    const auto groups = (command.instance_count + threads_per_group - 1u) / threads_per_group;
    context.command_buffer->dispatch(groups, 1u, 1u);
  }

  // Both culled buffers' writes above must land before depth_pre_pass/opaque_pass's indirect draws
  // (indirect_args) and vertex shader (transforms) read them -- same shape as light_culling_pass's
  // own compute-write-to-downstream-shader-read barrier.
  auto culled_ready = VkMemoryBarrier2{};
  culled_ready.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  culled_ready.srcStageMask = graphics::to_vk_enum<VkPipelineStageFlags2>(graphics::pipeline_stage::compute_shader);
  culled_ready.srcAccessMask = graphics::to_vk_enum<VkAccessFlags2>(graphics::access::shader_write);
  culled_ready.dstStageMask = graphics::to_vk_enum<VkPipelineStageFlags2>(graphics::pipeline_stage::draw_indirect | graphics::pipeline_stage::vertex_shader);
  culled_ready.dstAccessMask = graphics::to_vk_enum<VkAccessFlags2>(graphics::access::indirect_command_read | graphics::access::shader_read);
  context.command_buffer->memory_dependency(culled_ready);
}

} // namespace sbx::render
