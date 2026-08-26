// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/present_pass.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

struct present_push {
  std::uint32_t color_index;
  std::uint32_t sampler_index;
}; // struct present_push

present_pass::present_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();
  auto& surface = graphics_module.surface();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/passes/present.slang", entry_points});

  _pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {static_cast<graphics::format>(surface.format().format)},
    .cull_mode = graphics::cull_mode::none,
    .depth_test = false,
    .depth_write = false,
    .name = "Present"
  });
}

auto present_pass::execute(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& frame_context = graphics_module.frame_context();
  auto& swapchain = frame_context.swapchain();

  auto& scene = registry.get<graphics::image>(context.scene);

  // Scene image: tonemap's color writes -> this pass's sampled reads.
  auto to_read = graphics::command_buffer::image_transition_data{};
  to_read.image = scene;
  to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  to_read.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  to_read.dst_access_mask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  to_read.old_layout = graphics::image_layout::color_attachment_optimal;
  to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
  to_read.aspect_mask = scene.aspect();
  to_read.layer_count = 1u;
  context.command_buffer->transition_image_layout(to_read);

  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = swapchain.active_image_view();
  color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // fullscreen triangle overwrites everything
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  auto rendering_info = VkRenderingInfo{};
  rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{context.swapchain_extent.x(), context.swapchain_extent.y()}};
  rendering_info.layerCount = 1u;
  rendering_info.colorAttachmentCount = 1u;
  rendering_info.pColorAttachments = &color_attachment;

  context.command_buffer->begin_rendering(rendering_info);
  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  auto values = present_push{context.scene_index, context.sampler_index};
  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &values, sizeof(values));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  context.command_buffer->draw(3u, 1u, 0u, 0u);

  context.command_buffer->end_rendering();
}

} // namespace sbx::render
