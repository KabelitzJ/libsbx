// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/skinning/skin_pass.hpp>

#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

namespace {

// Compute-write (this pass) -> vertex-read (depth_pre_pass/shadow_pass/opaque_pass), all within
// the same frame's submission -- the *cross-frame* hazard (this frame's dispatch racing the
// previous frame's still-in-flight vertex fetch of the same scratch buffer) is instead handled by
// skin_pass::execute's frame_context::add_wait call below, same as particle_simulate_pass's pools.
auto skin_to_vertex_barrier() -> VkMemoryBarrier2 {
  auto barrier = VkMemoryBarrier2{};

  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  barrier.srcStageMask = graphics::to_vk_enum<VkPipelineStageFlags2>(graphics::pipeline_stage::compute_shader);
  barrier.srcAccessMask = graphics::to_vk_enum<VkAccessFlags2>(graphics::access::shader_write);
  barrier.dstStageMask = graphics::to_vk_enum<VkPipelineStageFlags2>(graphics::pipeline_stage::vertex_shader);
  barrier.dstAccessMask = graphics::to_vk_enum<VkAccessFlags2>(graphics::access::shader_read);

  return barrier;
}

} // namespace

skin_pass::skin_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto shader = shader_cache.get({"shaders/skinning/skin_vertices.slang", entry_points});

  _pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = shader,
    .name = "Skin Vertices"
  });
}

auto skin_pass::declare(compute_pass_builder&, const graph_resources&) -> void {
  // No cross-pass declaration needed -- hand-off to depth_pre_pass/shadow_pass/opaque_pass is a
  // plain VkMemoryBarrier2 at the end of execute(), same shape as particle_simulate_pass's hand-off
  // to particle_pass.
}

struct skin_push_data {
  graphics::buffer::address_type source_vertices;
  graphics::buffer::address_type source_skin_vertices;
  graphics::buffer::address_type palette;
  graphics::buffer::address_type output_vertices;
  std::uint32_t joint_offset;
  std::uint32_t vertex_count;
}; // struct skin_push_data

auto skin_pass::execute(render_context& context) -> void {
  if (context.packet->skin_dispatches.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& frame_context = graphics_module.frame_context();

  // The scratch output buffer is single-buffered (not per-frame-in-flight, unlike the joint
  // palette) since it's written and read entirely GPU-side within one frame's submission -- the
  // only hazard is *across* frames, so wait for the previous frame's compute stage the same way
  // particle_simulate_pass protects its own device_local pool buffers.
  frame_context.add_wait(frame_context.timeline(), frame_context.previous_frame_value(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  bind_compute_globals(context);

  auto& command_buffer = *context.command_buffer;
  command_buffer.bind_pipeline(*_pipeline);

  static constexpr auto threads_per_group = std::uint32_t{64u};

  for (const auto& dispatch : context.packet->skin_dispatches) {
    if (dispatch.vertex_count == 0u) {
      continue;
    }

    const auto data = skin_push_data{
      dispatch.source_vertex_address,
      dispatch.source_skin_vertex_address,
      context.joint_palette_address,
      dispatch.output_vertex_address,
      dispatch.joint_offset,
      dispatch.vertex_count
    };

    write_push_constants(context, data);

    const auto groups = (dispatch.vertex_count + threads_per_group - 1u) / threads_per_group;
    command_buffer.dispatch(groups, 1u, 1u);
  }

  auto barrier = skin_to_vertex_barrier();
  command_buffer.memory_dependency(barrier);
}

} // namespace sbx::render
