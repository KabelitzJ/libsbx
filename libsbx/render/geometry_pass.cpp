// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/geometry_pass.hpp>

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

geometry_pass::geometry_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/pbr/geometry.slang", entry_points});

  const auto make = [&](bool is_transparent, graphics::cull_mode cull, const std::string& name) {
    auto info = graphics::graphics_pipeline::create_info{
      .shader = shader,
      .color_formats = {render_pass::hdr_format},
      .depth_format = graphics::format::d32_sfloat,
      .cull_mode = cull,
      .front_face = graphics::front_face::counter_clockwise,
      .depth_test = true,
      .depth_write = false,
      .depth_compare = graphics::compare_operation::less_or_equal,
      .name = name
    };

    if (is_transparent) {
      info.color_blend_attachments = {graphics::blend_attachment{
        .enable = true,
        .source_color = graphics::blend_factor::source_alpha,
        .destination_color = graphics::blend_factor::one_minus_source_alpha,
        .color_operation = graphics::blend_operation::add,
        .source_alpha = graphics::blend_factor::one,
        .destination_alpha = graphics::blend_factor::one_minus_source_alpha,
        .alpha_operation = graphics::blend_operation::add
      }};
    }

    return pipeline_cache.get(info);
  };

  _opaque_pipelines[0] = make(false, graphics::cull_mode::back, "Mesh Opaque");
  _opaque_pipelines[1] = make(false, graphics::cull_mode::none, "Mesh Opaque Double-Sided");
  _transparent_pipelines[0] = make(true, graphics::cull_mode::back, "Mesh Transparent");
  _transparent_pipelines[1] = make(true, graphics::cull_mode::none, "Mesh Transparent Double-Sided");
}

auto geometry_pass::execute(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& bindless_table = graphics_module.bindless_table();
  auto& registry = graphics_module.resource_registry();
  auto& frame_context = graphics_module.frame_context();
  auto& swapchain = frame_context.swapchain();

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& depth = registry.get<graphics::image>(context.depth);
  auto& color = registry.get<graphics::image>(context.color);

  auto to_color = graphics::command_buffer::image_transition_data{};
  to_color.image = color;
  to_color.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color.src_access_mask = VK_ACCESS_2_NONE;
  to_color.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_color.old_layout = graphics::image_layout::undefined;
  to_color.new_layout = graphics::image_layout::color_attachment_optimal;
  to_color.aspect_mask = color.aspect();
  to_color.layer_count = 1u;
  context.command_buffer->transition_image_layout(to_color);

  auto depth_barrier = graphics::command_buffer::image_transition_data{};
  depth_barrier.image = depth;
  depth_barrier.src_stage_mask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
  depth_barrier.src_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  depth_barrier.dst_stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
  depth_barrier.dst_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  depth_barrier.old_layout = graphics::image_layout::depth_attachment_optimal;
  depth_barrier.new_layout = graphics::image_layout::depth_attachment_optimal;
  depth_barrier.aspect_mask = depth.aspect();
  depth_barrier.layer_count = 1u;
  context.command_buffer->transition_image_layout(depth_barrier);

  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = color.view();
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.clearValue.color = VkClearColorValue{{context.packet->clear_color.r(), context.packet->clear_color.g(), context.packet->clear_color.b(), context.packet->clear_color.a()}};

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

  submit_draw_commands(context, context.packet->opaque_commands, _opaque_pipelines);
  submit_draw_commands(context, context.packet->transparent_commands, _transparent_pipelines);

  context.command_buffer->end_rendering();
}

} // namespace sbx::render
