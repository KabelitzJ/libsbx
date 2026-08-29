// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/scene_blit_compositor.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/devices/surface.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>
#include <libsbx/graphics/bindless_table.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

namespace sbx::render {

struct scene_blit_push {
  std::uint32_t color_index;
  std::uint32_t sampler_index;
}; // struct scene_blit_push

scene_blit_compositor::scene_blit_compositor(scene_renderer_module& owner)
: _owner{owner} {
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
    .name = "Scene Blit"
  });
}

auto scene_blit_compositor::execute(compositor_context& context) -> void {
  if (!_owner.has_rendered()) {
    // Nothing rendered this frame (final_image may be stale or never written) — clear and
    // present the swapchain as-is rather than leaving it undefined.
    clear_swapchain(*context.command_buffer, context.swapchain_view, context.swapchain_extent);
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  // final_image is already shader_read_only_optimal by the time any compositor runs — see
  // scene_renderer_module::record.

  auto color_attachment = VkRenderingAttachmentInfo{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.imageView = context.swapchain_view;
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

  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(*context.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  const auto viewport = VkViewport{0.0f, 0.0f, static_cast<std::float_t>(context.swapchain_extent.x()), static_cast<std::float_t>(context.swapchain_extent.y()), 0.0f, 1.0f};
  context.command_buffer->set_viewport(viewport);

  const auto scissor = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{context.swapchain_extent.x(), context.swapchain_extent.y()}};
  context.command_buffer->set_scissor(scissor);

  context.command_buffer->bind_pipeline(*_pipeline);

  auto values = scene_blit_push{_owner.final_image_index(), _owner.sampler_index()};
  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &values, sizeof(values));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  context.command_buffer->draw(3u, 1u, 0u, 0u);

  context.command_buffer->end_rendering();
}

} // namespace sbx::render
