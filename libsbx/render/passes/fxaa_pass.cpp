// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/fxaa_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/memory/bytes.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

// Mirrors shaders/passes/fxaa.slang's push_data.
struct fxaa_push {
  std::uint32_t source_index;
  std::uint32_t sampler_index;
  std::float_t inverse_width;
  std::float_t inverse_height;
  std::uint32_t enabled;
}; // struct fxaa_push

fxaa_pass::fxaa_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();
  auto& surface = graphics_module.surface();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"engine://shaders/passes/fxaa.slang", entry_points});

  _pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {static_cast<graphics::format>(surface.format().format)},
    .cull_mode = graphics::cull_mode::none,
    .depth_test = false,
    .depth_write = false,
    .name = "FXAA"
  });
}

auto fxaa_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  builder.reads_image(resources.ldr, graphics::pipeline_stage::fragment_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);

  auto group = render_attachment_group{.extent = resources.extent};

  group.colors.push_back(color_attachment_slot{
    .image = resources.final_image,
    .store_op = graphics::attachment_store_op::store
  });

  builder.add_group(group);
}

auto fxaa_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  SBX_PROFILE_SCOPE("fxaa_pass::execute");
  SBX_STATS_SCOPE("fxaa_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "fxaa_pass::execute");

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& bindless_table = core::engine::get_module<graphics::graphics_module>().bindless_table();

  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  const auto values = fxaa_push{
    .source_index = context.ldr_index,
    .sampler_index = context.clamp_sampler_index,
    .inverse_width = 1.0f / static_cast<std::float_t>(std::max(context.extent.x(), 1u)),
    .inverse_height = 1.0f / static_cast<std::float_t>(std::max(context.extent.y(), 1u)),
    .enabled = context.packet->camera.post_process.anti_aliasing == scenes::post_anti_aliasing::fxaa ? 1u : 0u
  };

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, memory::as_bytes(values));

  context.command_buffer->draw(3u, 1u, 0u, 0u);
}

} // namespace sbx::render
