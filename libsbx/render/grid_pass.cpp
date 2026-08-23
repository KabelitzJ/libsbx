// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/grid_pass.hpp>

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

struct grid_push {
  graphics::buffer::address_type frame_address;
}; // struct grid_push

grid_pass::grid_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/pbr/grid.slang", entry_points});

  auto info = graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {render_pass::hdr_format},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = graphics::cull_mode::none,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .name = "Grid"
  };

  // Fades to transparent with distance/grazing angle — same "over" blend as transparent_pass.
  info.color_blend_attachments = {graphics::blend_attachment{
    .enable = true,
    .source_color = graphics::blend_factor::source_alpha,
    .destination_color = graphics::blend_factor::one_minus_source_alpha,
    .color_operation = graphics::blend_operation::add,
    .source_alpha = graphics::blend_factor::one,
    .destination_alpha = graphics::blend_factor::one_minus_source_alpha,
    .alpha_operation = graphics::blend_operation::add
  }};

  _pipeline = pipeline_cache.get(info);
}

auto grid_pass::execute(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (!context.show_grid || !context.packet->camera.is_active) {
    return;
  }

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();

  auto& color = registry.get<graphics::image>(context.color);
  auto& depth = registry.get<graphics::image>(context.depth);
  auto& color_msaa = registry.get<graphics::image>(context.color_msaa);

  // Continuation barriers, not fresh transitions — same reasoning as transparent_pass, which
  // runs in the same "after something already wrote color this frame" position (skybox_pass, in
  // this case, immediately before).
  auto to_color = graphics::command_buffer::image_transition_data{};
  to_color.image = color_msaa;
  to_color.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_color.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  to_color.old_layout = graphics::image_layout::color_attachment_optimal;
  to_color.new_layout = graphics::image_layout::color_attachment_optimal;
  to_color.aspect_mask = color_msaa.aspect();
  to_color.layer_count = 1u;
  context.command_buffer->transition_image_layout(to_color);

  auto to_resolve = graphics::command_buffer::image_transition_data{};
  to_resolve.image = color;
  to_resolve.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_resolve.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_resolve.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_resolve.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_resolve.old_layout = graphics::image_layout::color_attachment_optimal;
  to_resolve.new_layout = graphics::image_layout::color_attachment_optimal;
  to_resolve.aspect_mask = color.aspect();
  to_resolve.layer_count = 1u;
  context.command_buffer->transition_image_layout(to_resolve);

  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = color_msaa.view();
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
  color_attachment.resolveImageView = color.view();
  color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  auto depth_attachment = VkRenderingAttachmentInfo{};
  depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depth_attachment.imageView = depth.view();
  depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{context.extent.x(), context.extent.y()}};
  rendering_info.layerCount = 1u;
  rendering_info.colorAttachmentCount = 1u;
  rendering_info.pColorAttachments = &color_attachment;
  rendering_info.pDepthAttachment = &depth_attachment;

  context.command_buffer->begin_rendering(rendering_info);
  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  auto values = grid_push{context.frame_address};

  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &values, sizeof(values));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  context.command_buffer->draw(6u, 1u, 0u, 0u);

  context.command_buffer->end_rendering();
}

} // namespace sbx::render
