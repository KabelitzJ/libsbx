// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/ambient_occlusion_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

#include <algorithm>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

inline constexpr auto threads_per_group = std::uint32_t{8u};

// Mirrors shaders/passes/ambient_occlusion.slang's push_data (one layout for both entry points).
struct ambient_occlusion_push {
  graphics::buffer::address_type frame_address;
  std::uint32_t depth_index;
  std::uint32_t input_index;   // blur: the raw result
  std::uint32_t output_index;  // storage index
  std::float_t radius;
  std::float_t intensity;
  std::uint32_t samples;
}; // struct ambient_occlusion_push

auto transition(graphics::command_buffer& command_buffer, graphics::image& image, graphics::image_layout from, graphics::image_layout to, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) -> void {
  auto data = graphics::command_buffer::image_transition_data{};
  data.image = image.handle();
  data.src_stage_mask = src_stage;
  data.src_access_mask = src_access;
  data.dst_stage_mask = dst_stage;
  data.dst_access_mask = dst_access;
  data.old_layout = from;
  data.new_layout = to;
  data.aspect_mask = image.aspect();
  data.mip_levels = 1u;
  command_buffer.transition_image_layout(data);
}

ambient_occlusion_pass::ambient_occlusion_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  const auto occlusion_shader = shader_cache.get({"engine://shaders/passes/ambient_occlusion.slang", std::vector<graphics::shader_compiler::entry_point_request>{{VK_SHADER_STAGE_COMPUTE_BIT, "occlusion_main"}}});

  _occlusion_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = occlusion_shader,
    .name = "Ambient Occlusion"
  });

  const auto blur_shader = shader_cache.get({"engine://shaders/passes/ambient_occlusion.slang", std::vector<graphics::shader_compiler::entry_point_request>{{VK_SHADER_STAGE_COMPUTE_BIT, "blur_main"}}});

  _blur_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = blur_shader,
    .name = "Ambient Occlusion Blur"
  });
}

auto ambient_occlusion_pass::declare(compute_pass_builder& builder, const graph_resources& resources) -> void {
  builder.reads_image(resources.scene_depth, graphics::pipeline_stage::compute_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);

  // Both targets are transitioned by hand; only the blurred result's final state is declared, for opaque_pass.
  builder.declares_image_ready(resources.ambient_occlusion, graphics::pipeline_stage::fragment_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);
}

auto ambient_occlusion_pass::execute(render_context& context) -> void {
  SBX_PROFILE_SCOPE("ambient_occlusion_pass::execute");
  SBX_STATS_SCOPE("ambient_occlusion_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "ambient_occlusion_pass::execute");

  if (!context.ambient_occlusion_raw.is_valid() || !context.ambient_occlusion.is_valid()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  auto& command_buffer = *context.command_buffer;

  auto& raw = registry.get<graphics::image>(context.ambient_occlusion_raw);
  auto& blurred = registry.get<graphics::image>(context.ambient_occlusion);

  // Regenerated every frame, so undefined -> general is valid; still waits on the previous frame's reads.
  for (auto* image : {&raw, &blurred}) {
    transition(command_buffer, *image, graphics::image_layout::undefined, graphics::image_layout::general,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
      VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT);
  }

  const auto& settings = context.packet->camera.post_process.ambient_occlusion;

  if (!settings.enabled || !context.packet->camera.is_active) {
    // Nothing samples them this frame, but opaque_pass's declared read expects read-only layouts.
    for (auto* image : {&raw, &blurred}) {
      transition(command_buffer, *image, graphics::image_layout::general, graphics::image_layout::shader_read_only_optimal,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    }

    return;
  }

  bind_compute_globals(context);

  const auto extent = math::vector2u{std::max(context.extent.x() / 2u, 1u), std::max(context.extent.y() / 2u, 1u)};
  const auto groups_x = (extent.x() + threads_per_group - 1u) / threads_per_group;
  const auto groups_y = (extent.y() + threads_per_group - 1u) / threads_per_group;

  auto data = ambient_occlusion_push{
    .frame_address = context.frame_address,
    .depth_index = context.scene_depth_index,
    .input_index = 0u,
    .output_index = context.ambient_occlusion_raw_storage_index,
    .radius = std::max(settings.radius, 0.001f),
    .intensity = std::max(settings.intensity, 0.0f),
    .samples = std::clamp(settings.samples, 1u, 32u)
  };

  command_buffer.bind_pipeline(*_occlusion_pipeline);
  write_push_constants(context, data);
  command_buffer.dispatch(groups_x, groups_y, 1u);

  transition(command_buffer, raw, graphics::image_layout::general, graphics::image_layout::shader_read_only_optimal,
    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);

  data.input_index = context.ambient_occlusion_raw_index;
  data.output_index = context.ambient_occlusion_storage_index;

  command_buffer.bind_pipeline(*_blur_pipeline);
  write_push_constants(context, data);
  command_buffer.dispatch(groups_x, groups_y, 1u);

  transition(command_buffer, blurred, graphics::image_layout::general, graphics::image_layout::shader_read_only_optimal,
    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
}

} // namespace sbx::render
