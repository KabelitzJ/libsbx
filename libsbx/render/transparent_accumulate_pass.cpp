// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/transparent_accumulate_pass.hpp>

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

transparent_accumulate_pass::transparent_accumulate_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main", "alpha_blend_shading_policy"}
  };

  const auto& shader = shader_cache.get({"shaders/pbr/geometry.slang", entry_points});

  const auto make = [&](graphics::cull_mode cull, const std::string& name) {
    auto info = graphics::graphics_pipeline::create_info{
      .shader = shader,
      .color_formats = {render_pass::hdr_format, graphics::format::r16_sfloat},
      .depth_format = graphics::format::d32_sfloat,
      .cull_mode = cull,
      .front_face = graphics::front_face::counter_clockwise,
      .depth_test = true,
      .depth_write = false,
      .depth_compare = graphics::compare_operation::less_or_equal,
      .samples = render_pass::sample_count,
      .name = name
    };

    info.color_blend_attachments = {
      // Accumulator: additive — sum of weight * premultiplied(color, alpha) across every
      // fragment that lands here, order-independent.
      graphics::blend_attachment{
        .enable = true,
        .source_color = graphics::blend_factor::one,
        .destination_color = graphics::blend_factor::one,
        .color_operation = graphics::blend_operation::add,
        .source_alpha = graphics::blend_factor::one,
        .destination_alpha = graphics::blend_factor::one,
        .alpha_operation = graphics::blend_operation::add
      },
      // Revealage: multiplicative — dst *= (1 - alpha), the classic
      // glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR) McGuire/Bavoil recipe.
      graphics::blend_attachment{
        .enable = true,
        .source_color = graphics::blend_factor::zero,
        .destination_color = graphics::blend_factor::one_minus_source_color,
        .color_operation = graphics::blend_operation::add,
        .source_alpha = graphics::blend_factor::zero,
        .destination_alpha = graphics::blend_factor::one_minus_source_color,
        .alpha_operation = graphics::blend_operation::add
      }
    };

    return pipeline_cache.get(info);
  };

  _pipelines[0] = make(graphics::cull_mode::back, "Transparent Accumulate");
  _pipelines[1] = make(graphics::cull_mode::none, "Transparent Accumulate Double-Sided");
}

auto transparent_accumulate_pass::execute(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& registry = graphics_module.resource_registry();

  auto& depth = registry.get<graphics::image>(context.depth);
  auto& accum_msaa = registry.get<graphics::image>(context.accum_msaa);
  auto& accum = registry.get<graphics::image>(context.accum);
  auto& reveal_msaa = registry.get<graphics::image>(context.reveal_msaa);
  auto& reveal = registry.get<graphics::image>(context.reveal);

  // Fresh-write barriers, not continuation ones — these targets are cleared and written for the
  // first time this frame, same pattern as opaque_pass's color_msaa/color barriers. No depth
  // barrier needed: depth has stayed in depth_attachment_optimal since opaque_pass, same reasoning
  // skybox_pass documents for itself.
  const auto fresh_write_barrier = [&](graphics::image& image) {
    auto barrier = graphics::command_buffer::image_transition_data{};
    barrier.image = image;
    barrier.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.src_access_mask = VK_ACCESS_2_NONE;
    barrier.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.old_layout = graphics::image_layout::undefined;
    barrier.new_layout = graphics::image_layout::color_attachment_optimal;
    barrier.aspect_mask = image.aspect();
    barrier.layer_count = 1u;
    context.command_buffer->transition_image_layout(barrier);
  };

  fresh_write_barrier(accum_msaa);
  fresh_write_barrier(accum);
  fresh_write_barrier(reveal_msaa);
  fresh_write_barrier(reveal);

  auto accum_attachment = VkRenderingAttachmentInfo{};
  accum_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  accum_attachment.imageView = accum_msaa.view();
  accum_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  accum_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
  accum_attachment.resolveImageView = accum.view();
  accum_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  accum_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  accum_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  accum_attachment.clearValue.color = VkClearColorValue{{0.0f, 0.0f, 0.0f, 0.0f}};

  auto reveal_attachment = VkRenderingAttachmentInfo{};
  reveal_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  reveal_attachment.imageView = reveal_msaa.view();
  reveal_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  reveal_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
  reveal_attachment.resolveImageView = reveal.view();
  reveal_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  reveal_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  reveal_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  // Revealage starts at 1 ("fully see-through" / background 100% visible) and decreases
  // multiplicatively as transparent layers accumulate — see the blend state above.
  reveal_attachment.clearValue.color = VkClearColorValue{{1.0f, 0.0f, 0.0f, 0.0f}};

  const auto color_attachments = std::array{accum_attachment, reveal_attachment};

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
  rendering_info.colorAttachmentCount = static_cast<std::uint32_t>(color_attachments.size());
  rendering_info.pColorAttachments = color_attachments.data();
  rendering_info.pDepthAttachment = &depth_attachment;

  context.command_buffer->begin_rendering(rendering_info);

  bind_globals(context);

  submit_draw_commands(context, context.packet->transparent_commands, _pipelines);

  context.command_buffer->end_rendering();
}

} // namespace sbx::render
