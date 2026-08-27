// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/depth_pre_pass.hpp>

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

depth_pre_pass::depth_pre_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/passes/depth_pre.slang", entry_points});

  const auto make = [&](graphics::cull_mode cull, const std::string& name) {
    return pipeline_cache.get(graphics::graphics_pipeline::create_info{
      .shader = shader,
      .color_formats = {},
      .depth_format = graphics::format::d32_sfloat,
      .cull_mode = cull,
      .front_face = graphics::front_face::counter_clockwise,
      .depth_test = true,
      .depth_write = true,
      .depth_compare = graphics::compare_operation::less_or_equal,
      .samples = render_pass::sample_count,
      .name = name
    });
  };

  _pipelines[0] = make(graphics::cull_mode::back, "Depth Pre");
  _pipelines[1] = make(graphics::cull_mode::none, "Depth Pre Double-Sided");
}

auto depth_pre_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  auto group = render_attachment_group{.extent = resources.extent};

  group.depth = depth_attachment_slot{
    .image = resources.depth,
    .access_mask = graphics::access::depth_stencil_attachment_write | graphics::access::depth_stencil_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .clear_value = graphics::depth_stencil_clear_value{1.0f, 0u}
  };

  builder.add_group(group);
}

auto depth_pre_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  if (!context.packet->camera.is_active) {
    return;
  }

  bind_globals(context);
  submit_draw_commands(context, context.packet->opaque_commands, _pipelines);
}

} // namespace sbx::render
