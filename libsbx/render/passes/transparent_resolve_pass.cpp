// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/transparent_resolve_pass.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

struct transparent_resolve_push {
  std::uint32_t accumulator_index;
  std::uint32_t revealage_index;
  std::uint32_t sampler_index;
}; // struct transparent_resolve_push

transparent_resolve_pass::transparent_resolve_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/pbr/transparent_resolve.slang", entry_points});

  // Draws straight into the single-sample HDR color target — no `.samples` override here (default
  // count_1), unlike transparent_accumulate_pass which targets the MSAA pair.
  _pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {render_pass::hdr_format},
    .cull_mode = graphics::cull_mode::none,
    .depth_test = false,
    .depth_write = false,
    .color_blend_attachments = {graphics::blend_attachment{
      .enable = true,
      .source_color = graphics::blend_factor::source_alpha,
      .destination_color = graphics::blend_factor::one_minus_source_alpha,
      .color_operation = graphics::blend_operation::add,
      .source_alpha = graphics::blend_factor::one,
      .destination_alpha = graphics::blend_factor::one_minus_source_alpha,
      .alpha_operation = graphics::blend_operation::add
    }},
    .name = "Transparent Resolve"
  });
}

auto transparent_resolve_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  // accumulator/revealage: transparent_accumulate_pass's writes -> this pass's sampled reads.
  builder.reads_image(resources.accumulator, graphics::pipeline_stage::fragment_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);
  builder.reads_image(resources.revealage, graphics::pipeline_stage::fragment_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);

  auto group = render_attachment_group{.extent = resources.extent};

  // Continuation write, not a fresh transition — color is already color_attachment_optimal
  // (grid_pass wrote it this frame); this pass both blend-reads and writes it.
  group.colors.push_back(color_attachment_slot{
    .image = resources.color,
    .access_mask = graphics::access::color_attachment_write | graphics::access::color_attachment_read,
    .store_op = graphics::attachment_store_op::store
  });

  builder.add_group(group);
}

auto transparent_resolve_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& bindless_table = graphics_module.bindless_table();

  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  auto values = transparent_resolve_push{context.accumulator_index, context.revealage_index, context.sampler_index};
  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &values, sizeof(values));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  context.command_buffer->draw(3u, 1u, 0u, 0u);
}

} // namespace sbx::render
