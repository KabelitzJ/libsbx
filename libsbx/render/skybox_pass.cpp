// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/skybox_pass.hpp>

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

struct skybox_push {
  math::matrix4x4 inverse_view_projection;
  math::vector4 camera_position;
  std::uint32_t environment_index;
  std::uint32_t sampler_index;
}; // struct skybox_push

skybox_pass::skybox_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/pbr/skybox.slang", entry_points});

  const auto make = [&](graphics::cull_mode cull, const std::string& name) {
    return pipeline_cache.get(graphics::graphics_pipeline::create_info{
      .shader = shader,
      .color_formats = {render_pass::hdr_format},
      .depth_format = graphics::format::d32_sfloat,
      .cull_mode = graphics::cull_mode::none,
      .depth_test = true,
      .depth_write = false,
      .depth_compare = graphics::compare_operation::less_or_equal,
      .name = "Skybox"
    });
  };

  _pipeline = make(graphics::cull_mode::back, "Skybox");
}

auto skybox_pass::execute(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();

  auto& color = registry.get<graphics::image>(context.color);
  auto& depth = registry.get<graphics::image>(context.depth);

  // Order geometry's color writes before this pass's color writes (same HDR attachment).
  auto color_barrier = graphics::command_buffer::image_transition_data{};
  color_barrier.image = color;
  color_barrier.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  color_barrier.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  color_barrier.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  color_barrier.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  color_barrier.old_layout = graphics::image_layout::color_attachment_optimal;
  color_barrier.new_layout = graphics::image_layout::color_attachment_optimal;
  color_barrier.aspect_mask = color.aspect();
  color_barrier.layer_count = 1u;
  context.command_buffer->transition_image_layout(color_barrier);

  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = color.view();
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;   // keep the geometry the pass already drew
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  auto depth_attachment = VkRenderingAttachmentInfo{};
  depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depth_attachment.imageView = depth.view();
  depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;   // test against the scene depth, don't write
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

  auto values = skybox_push{context.inverse_view_projection, math::vector4{context.packet->camera.position, 1.0f}, context.environment_index, context.sampler_index};
  
  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &values, sizeof(values));
  
  context.command_buffer->push_constants(bindless_table.pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0u, range);

  context.command_buffer->draw(3u, 1u, 0u, 0u);

  context.command_buffer->end_rendering();
}

} // namespace sbx::render
