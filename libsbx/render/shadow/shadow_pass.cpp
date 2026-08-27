// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/shadow/shadow_pass.hpp>

#include <array>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

shadow_pass::shadow_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/passes/shadow.slang", entry_points});

  const auto make = [&](graphics::cull_mode cull, const std::string& name) {
    return pipeline_cache.get(graphics::graphics_pipeline::create_info{
      .shader = shader,
      .color_formats = {},
      .depth_format = graphics::format::d32_sfloat,
      .cull_mode = cull,
      .front_face = graphics::front_face::counter_clockwise,
      .depth_bias = graphics::depth_bias{
        .constant_factor = 1.5f, 
        .slope_factor = 2.5f, 
        .clamp = 0.0f
      },
      .depth_test = true,
      .depth_write = true,
      .depth_compare = graphics::compare_operation::less_or_equal,
      .samples = graphics::samples::count_1,
      .name = name
    });
  };

  _pipelines[0] = make(graphics::cull_mode::back, "Shadow Cascade");
  _pipelines[1] = make(graphics::cull_mode::none, "Shadow Cascade Double-Sided");
}

auto shadow_pass::execute(render_context& context) -> void {
  if (!context.has_shadow_caster) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  const auto shadow_extent = math::vector2u{shadow_map_resolution, shadow_map_resolution};

  for (auto cascade = std::uint32_t{0u}; cascade < shadow_cascade_count; ++cascade) {
    auto& shadow_map = registry.get<graphics::image>(context.shadow_maps[cascade]);

    auto to_depth = graphics::command_buffer::image_transition_data{};
    to_depth.image = shadow_map.handle();
    to_depth.src_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    to_depth.src_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_depth.dst_stage_mask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    to_depth.dst_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    to_depth.old_layout = graphics::image_layout::undefined;
    to_depth.new_layout = graphics::image_layout::depth_attachment_optimal;
    to_depth.aspect_mask = shadow_map.aspect();
    to_depth.layer_count = 1u;
    context.command_buffer->transition_image_layout(to_depth);

    auto depth_attachment = VkRenderingAttachmentInfo{};
    depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_attachment.imageView = shadow_map.view();
    depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.clearValue.depthStencil = VkClearDepthStencilValue{1.0f, 0u};

    auto rendering_info = VkRenderingInfo{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.renderArea = VkRect2D{VkOffset2D{0, 0}, VkExtent2D{shadow_extent.x(), shadow_extent.y()}};
    rendering_info.layerCount = 1u;
    rendering_info.colorAttachmentCount = 0u;
    rendering_info.pColorAttachments = nullptr;
    rendering_info.pDepthAttachment = &depth_attachment;

    context.command_buffer->begin_rendering(rendering_info);

    bind_globals(context, shadow_extent);
    submit_draw_commands(context, context.packet->shadow_caster_commands, _pipelines, cascade);

    context.command_buffer->end_rendering();

    auto to_read = graphics::command_buffer::image_transition_data{};
    to_read.image = shadow_map.handle();
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    to_read.src_access_mask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = graphics::image_layout::depth_attachment_optimal;
    to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
    to_read.aspect_mask = shadow_map.aspect();
    to_read.layer_count = 1u;
    context.command_buffer->transition_image_layout(to_read);
  }
}

} // namespace sbx::render
