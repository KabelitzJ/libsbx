// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/particle_draw_pass.hpp>

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

namespace {

struct push_data {
  graphics::buffer::address_type frame;
  graphics::buffer::address_type particles;
  graphics::buffer::address_type alive_list;
  graphics::buffer::address_type emitters;
  std::uint32_t sampler_index;
}; // struct push_data

auto push(render_context& context, const push_data& data) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &data, sizeof(data));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);
}

// Continuation barrier, not a fresh transition — the target already holds this frame's content
// (color: skybox/grid; accum/reveal: transparent_accumulate_pass) and this pass adds to it.
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

} // namespace

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

  // Exactly transparent_accumulate_pass's blend state — same accumulator/revealage targets, same
  // McGuire/Bavoil weight function (see draw.slang's alpha_blend_shading_policy), so
  // transparent_resolve_pass composites mesh transparency and alpha-blend particles together
  // without needing to know they came from different passes.
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

  // pool[0] additive — straight into the shared color target, ahead of transparent_resolve_pass so
  // it composites underneath whatever mesh/particle transparency resolves on top next.
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
    push(context, push_data{context.frame_address, context.particle_additive_particles_address, context.particle_additive_alive_list_address, context.particle_additive_emitters_address, context.sampler_index});

    auto& draw_args = registry.get<graphics::buffer>(context.particle_additive_draw_args);
    context.command_buffer->draw_indirect(draw_args, 0u, 1u);

    context.command_buffer->end_rendering();
  }

  // pool[1] alpha blend — into the same WBOIT accum/reveal targets transparent_accumulate_pass just
  // wrote, as a continuation (loadOp LOAD, not CLEAR): mesh transparency and particles share one
  // accumulator/revealage pair, resolved together by transparent_resolve_pass right after this pass.
  if (context.particle_alpha_draw_args.is_valid()) {
    auto& accum = registry.get<graphics::image>(context.accum);
    auto& accum_msaa = registry.get<graphics::image>(context.accum_msaa);
    auto& reveal = registry.get<graphics::image>(context.reveal);
    auto& reveal_msaa = registry.get<graphics::image>(context.reveal_msaa);

    auto to_accum_msaa = continuation_write_barrier(accum_msaa);
    context.command_buffer->transition_image_layout(to_accum_msaa);

    auto to_reveal_msaa = continuation_write_barrier(reveal_msaa);
    context.command_buffer->transition_image_layout(to_reveal_msaa);

    auto accum_attachment = VkRenderingAttachmentInfo{};
    accum_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    accum_attachment.imageView = accum_msaa.view();
    accum_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    accum_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    accum_attachment.resolveImageView = accum.view();
    accum_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    accum_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    accum_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    auto reveal_attachment = VkRenderingAttachmentInfo{};
    reveal_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    reveal_attachment.imageView = reveal_msaa.view();
    reveal_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    reveal_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    reveal_attachment.resolveImageView = reveal.view();
    reveal_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    reveal_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    reveal_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    const auto color_attachments = std::array{accum_attachment, reveal_attachment};

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
    push(context, push_data{context.frame_address, context.particle_alpha_particles_address, context.particle_alpha_alive_list_address, context.particle_alpha_emitters_address, context.sampler_index});

    auto& draw_args = registry.get<graphics::buffer>(context.particle_alpha_draw_args);
    context.command_buffer->draw_indirect(draw_args, 0u, 1u);

    context.command_buffer->end_rendering();
  }
}

} // namespace sbx::render
