// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/hiz_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

#include <algorithm>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/math/vector2.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

// Mirrors shaders/passes/hiz.slang's push_data.
struct hiz_push_data {
  std::uint32_t source_index;
  std::uint32_t destination_index;
  std::uint32_t source_level;
  std::uint32_t padding;
  math::vector2u source_size;
  math::vector2u size;
}; // struct hiz_push_data

inline constexpr auto hiz_threads_per_group = std::uint32_t{8u};

hiz_pass::hiz_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  _pipeline = graphics_module.compute_pipeline_cache().get(graphics::compute_pipeline::create_info{
    .shader = graphics_module.shader_cache().get({"engine://shaders/passes/hiz.slang", std::vector<graphics::shader_compiler::entry_point_request>{{VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}}}),
    .name = "HiZ"
  });
}

auto hiz_pass::declare(compute_pass_builder& builder, const graph_resources& resources) -> void {
  builder.reads_image(resources.scene_depth, graphics::pipeline_stage::compute_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);
  builder.writes_image(resources.hiz, graphics::pipeline_stage::compute_shader, graphics::access::shader_sampled_read | graphics::access::shader_storage_write, graphics::image_layout::general);
}

auto hiz_pass::execute(render_context& context) -> void {
  SBX_PROFILE_SCOPE("hiz_pass::execute");
  SBX_STATS_SCOPE("hiz_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "hiz_pass::execute");

  auto& command_buffer = *context.command_buffer;

  bind_compute_globals(context);
  command_buffer.bind_pipeline(*_pipeline);

  auto source_size = context.extent;

  for (auto level = std::uint32_t{0u}; level < hiz_mip_count; ++level) {
    const auto size = math::vector2u{std::max(hiz_width >> level, 1u), std::max(hiz_height >> level, 1u)};

    // Level 0 reduces the scene depth, every later level the one before it.
    const auto push = hiz_push_data{
      .source_index = level == 0u ? context.scene_depth_index : context.hiz_index,
      .destination_index = context.hiz_storage_indices[level],
      .source_level = level == 0u ? 0u : level - 1u,
      .padding = 0u,
      .source_size = source_size,
      .size = size
    };

    write_push_constants(context, push);
    command_buffer.dispatch((size.x() + hiz_threads_per_group - 1u) / hiz_threads_per_group, (size.y() + hiz_threads_per_group - 1u) / hiz_threads_per_group, 1u);

    // The next level reads this one; the last level's readers are ordered by the graph.
    if (level + 1u < hiz_mip_count) {
      auto barrier = VkMemoryBarrier2{};
      barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
      barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      barrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
      command_buffer.memory_dependency(barrier);
    }

    source_size = size;
  }
}

} // namespace sbx::render
