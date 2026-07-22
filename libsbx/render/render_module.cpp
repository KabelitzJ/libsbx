// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/render_module.hpp>

#include <array>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/profiler.hpp>

namespace sbx::render {

render_module::render_module() { }

render_module::~render_module() { }

auto render_module::render() -> void {
  SBX_PROFILE_SCOPE("render_module::render");

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
 
  auto& frame_context = graphics_module.frame_context();
  
  auto command_buffer = frame_context.begin_frame();
  
  if (!command_buffer) {
    return;
  }

  {
    SBX_PROFILE_GPU_SCOPE((*command_buffer), "render_module::render");

    _render_scene(*command_buffer, frame_context);
  }

  
  frame_context.end_frame();
}

auto render_module::_render_scene(graphics::command_buffer& command_buffer, graphics::frame_context& frame_context) -> void {
  SBX_PROFILE_SCOPE("render_module::_render_scene");

  SBX_PROFILE_GPU_SCOPE(command_buffer, "render_module::_render_scene");
  
  const auto& swapchain = frame_context.swapchain();
  
  auto to_color_attachment = sbx::graphics::command_buffer::image_transition_data{};
  to_color_attachment.image = swapchain.active_image();
  to_color_attachment.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color_attachment.src_access_mask = VK_ACCESS_2_NONE;
  to_color_attachment.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_color_attachment.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_color_attachment.old_layout = sbx::graphics::image_layout::undefined;
  to_color_attachment.new_layout = sbx::graphics::image_layout::color_attachment_optimal;
  
  command_buffer.transition_image_layout(to_color_attachment);
  
  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = swapchain.active_image_view();
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.clearValue.color = VkClearColorValue{{0.7f, 0.2f, 0.2f, 1.0f}};
  
  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, swapchain.extent()};
  rendering_info.layerCount = 1u;
  rendering_info.colorAttachmentCount = 1u;
  rendering_info.pColorAttachments = &color_attachment;
  
  command_buffer.begin_rendering(rendering_info);

  command_buffer.end_rendering();
  
  auto to_present_src = sbx::graphics::command_buffer::image_transition_data{};
  to_present_src.image = swapchain.active_image();
  to_present_src.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_present_src.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_present_src.dst_stage_mask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
  to_present_src.dst_access_mask = VK_ACCESS_2_NONE;
  to_present_src.old_layout = sbx::graphics::image_layout::color_attachment_optimal;
  to_present_src.new_layout = sbx::graphics::image_layout::present_source;
  
  command_buffer.transition_image_layout(to_present_src);
}

} // namespace sbx::render
