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

auto transparent_resolve_pass::execute(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();

  auto& color = registry.get<graphics::image>(context.color);
  auto& accumulator = registry.get<graphics::image>(context.accumulator);
  auto& revealage = registry.get<graphics::image>(context.revealage);

  // accumulator/revealage: transparent_accumulate_pass's writes -> this pass's sampled reads.
  const auto to_read = [&](graphics::image& image) {
    auto barrier = graphics::command_buffer::image_transition_data{};
    barrier.image = image;
    barrier.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dst_access_mask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    barrier.old_layout = graphics::image_layout::color_attachment_optimal;
    barrier.new_layout = graphics::image_layout::shader_read_only_optimal;
    barrier.aspect_mask = image.aspect();
    barrier.layer_count = 1u;
    context.command_buffer->transition_image_layout(barrier);
  };

  to_read(accumulator);
  to_read(revealage);

  // Continuation barrier, not a fresh transition — color is already color_attachment_optimal
  // (grid_pass wrote it this frame); this pass both blend-reads and writes it.
  auto to_color = graphics::command_buffer::image_transition_data{};
  to_color.image = color;
  to_color.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_color.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  to_color.old_layout = graphics::image_layout::color_attachment_optimal;
  to_color.new_layout = graphics::image_layout::color_attachment_optimal;
  to_color.aspect_mask = color.aspect();
  to_color.layer_count = 1u;
  context.command_buffer->transition_image_layout(to_color);

  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = color.view();
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{context.extent.x(), context.extent.y()}};
  rendering_info.layerCount = 1u;
  rendering_info.colorAttachmentCount = 1u;
  rendering_info.pColorAttachments = &color_attachment;

  context.command_buffer->begin_rendering(rendering_info);
  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  auto values = transparent_resolve_push{context.accumulator_index, context.revealage_index, context.sampler_index};
  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &values, sizeof(values));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  context.command_buffer->draw(3u, 1u, 0u, 0u);

  context.command_buffer->end_rendering();
}

} // namespace sbx::render
