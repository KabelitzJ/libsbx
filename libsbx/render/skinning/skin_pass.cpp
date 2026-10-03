// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/skinning/skin_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

skin_pass::skin_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto shader = shader_cache.get({"engine://shaders/skinning/skin_vertices.slang", entry_points});

  _pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = shader,
    .name = "Skin Vertices"
  });
}

auto skin_pass::declare(compute_pass_builder& builder, const graph_resources& resources) -> void {
  // The consumers declare their vertex-shader reads, so the graph places the barrier.
  builder.writes_buffer(resources.skin_scratch_buffer, graphics::pipeline_stage::compute_shader, graphics::access::shader_write);
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
  SBX_PROFILE_SCOPE("skin_pass::execute");
  SBX_STATS_SCOPE("skin_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "skin_pass::execute");

  if (context.packet->skin_dispatches.empty()) {
    return;
  }

  // No cross-frame wait: each frame slot writes its own scratch region.
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
}

} // namespace sbx::render
