// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/particles/particle_draw_pass.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

struct push_data {
  graphics::buffer::address_type frame;
  graphics::buffer::address_type particles;
  graphics::buffer::address_type alive_list;
  graphics::buffer::address_type emitters;
  std::uint32_t sampler_index;
}; // struct push_data

auto continuation_write_barrier(graphics::image& image) -> graphics::command_buffer::image_transition_data {
  auto barrier = graphics::command_buffer::image_transition_data{};
  barrier.image = image;
  barrier.src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  barrier.src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dst_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  barrier.dst_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  barrier.old_layout = graphics::image_layout::color_attachment_optimal;
  barrier.new_layout = graphics::image_layout::color_attachment_optimal;
  barrier.aspect_mask = image.aspect();
  barrier.layer_count = 1u;
  return barrier;
}

particle_draw_pass::particle_draw_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto additive_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main", "additive_shading_policy"}
  };

  const auto& additive_shader = shader_cache.get({"shaders/particles/draw.slang", additive_entry_points});

  _additive_pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = additive_shader,
    .color_formats = {render_pass::hdr_format},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = graphics::cull_mode::none,
    .front_face = graphics::front_face::counter_clockwise,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .color_blend_attachments = {graphics::blend_attachment{
      .enable = true,
      .source_color = graphics::blend_factor::one,
      .destination_color = graphics::blend_factor::one,
      .color_operation = graphics::blend_operation::add,
      .source_alpha = graphics::blend_factor::one,
      .destination_alpha = graphics::blend_factor::one,
      .alpha_operation = graphics::blend_operation::add
    }},
    .name = "Particle Additive"
  });

  const auto alpha_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main", "alpha_blend_shading_policy"}
  };

  const auto& alpha_shader = shader_cache.get({"shaders/particles/draw.slang", alpha_entry_points});

  _alpha_blend_pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = alpha_shader,
    .color_formats = {render_pass::hdr_format, graphics::format::r16_sfloat},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = graphics::cull_mode::none,
    .front_face = graphics::front_face::counter_clockwise,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .color_blend_attachments = {
      graphics::blend_attachment{
        .enable = true,
        .source_color = graphics::blend_factor::one,
        .destination_color = graphics::blend_factor::one,
        .color_operation = graphics::blend_operation::add,
        .source_alpha = graphics::blend_factor::one,
        .destination_alpha = graphics::blend_factor::one,
        .alpha_operation = graphics::blend_operation::add
      },
      graphics::blend_attachment{
        .enable = true,
        .source_color = graphics::blend_factor::zero,
        .destination_color = graphics::blend_factor::one_minus_source_color,
        .color_operation = graphics::blend_operation::add,
        .source_alpha = graphics::blend_factor::zero,
        .destination_alpha = graphics::blend_factor::one_minus_source_color,
        .alpha_operation = graphics::blend_operation::add
      }
    },
    .name = "Particle Alpha Blend"
  });
}

auto particle_draw_pass::execute(render_context& context) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& registry = graphics_module.resource_registry();

  auto& depth = registry.get<graphics::image>(context.depth);

  auto depth_attachment = VkRenderingAttachmentInfo{};
  depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depth_attachment.imageView = depth.view();
  depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  if (context.particle_additive_draw_args.is_valid()) {
    auto& color = registry.get<graphics::image>(context.color);
    auto& color_msaa = registry.get<graphics::image>(context.color_msaa);

    auto to_color_msaa = continuation_write_barrier(color_msaa);
    context.command_buffer->transition_image_layout(to_color_msaa);

    auto color_attachment = VkRenderingAttachmentInfo{};
    color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_attachment.imageView = color_msaa.view();
    color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    color_attachment.resolveImageView = color.view();
    color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    auto rendering_info = VkRenderingInfo{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{context.extent.x(), context.extent.y()}};
    rendering_info.layerCount = 1u;
    rendering_info.colorAttachmentCount = 1u;
    rendering_info.pColorAttachments = &color_attachment;
    rendering_info.pDepthAttachment = &depth_attachment;

    context.command_buffer->begin_rendering(rendering_info);
    bind_globals(context);

    context.command_buffer->bind_pipeline(*_additive_pipeline);

    auto data = push_data{
      context.frame_address, 
      context.particle_additive_particles_address, 
      context.particle_additive_alive_list_address, 
      context.particle_additive_emitters_address, 
      context.sampler_index
    };

    write_push_constants(context, data);

    auto& draw_args = registry.get<graphics::buffer>(context.particle_additive_draw_args);
    context.command_buffer->draw_indirect(draw_args, 0u, 1u);

    context.command_buffer->end_rendering();
  }

  if (context.particle_alpha_draw_args.is_valid()) {
    auto& accumulator = registry.get<graphics::image>(context.accumulator);
    auto& accumulator_msaa = registry.get<graphics::image>(context.accumulator_msaa);
    auto& revealage = registry.get<graphics::image>(context.revealage);
    auto& revealage_msaa = registry.get<graphics::image>(context.revealage_msaa);

    auto to_accum_msaa = continuation_write_barrier(accumulator_msaa);
    context.command_buffer->transition_image_layout(to_accum_msaa);

    auto to_revealage_msaa = continuation_write_barrier(revealage_msaa);
    context.command_buffer->transition_image_layout(to_revealage_msaa);

    auto accum_attachment = VkRenderingAttachmentInfo{};
    accum_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    accum_attachment.imageView = accumulator_msaa.view();
    accum_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    accum_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    accum_attachment.resolveImageView = accumulator.view();
    accum_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    accum_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    accum_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    auto revealage_attachment = VkRenderingAttachmentInfo{};
    revealage_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    revealage_attachment.imageView = revealage_msaa.view();
    revealage_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    revealage_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    revealage_attachment.resolveImageView = revealage.view();
    revealage_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    revealage_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    revealage_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    const auto color_attachments = std::array{accum_attachment, revealage_attachment};

    auto rendering_info = VkRenderingInfo{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{context.extent.x(), context.extent.y()}};
    rendering_info.layerCount = 1u;
    rendering_info.colorAttachmentCount = static_cast<std::uint32_t>(color_attachments.size());
    rendering_info.pColorAttachments = color_attachments.data();
    rendering_info.pDepthAttachment = &depth_attachment;

    context.command_buffer->begin_rendering(rendering_info);
    bind_globals(context);

    context.command_buffer->bind_pipeline(*_alpha_blend_pipeline);

    auto data = push_data{
      context.frame_address, 
      context.particle_alpha_particles_address, 
      context.particle_alpha_alive_list_address, 
      context.particle_alpha_emitters_address, 
      context.sampler_index
    };

    write_push_constants(context, data);

    auto& draw_args = registry.get<graphics::buffer>(context.particle_alpha_draw_args);
    context.command_buffer->draw_indirect(draw_args, 0u, 1u);

    context.command_buffer->end_rendering();
  }
}

} // namespace sbx::render
