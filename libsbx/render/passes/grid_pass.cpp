// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/grid_pass.hpp>

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

struct grid_push {
  graphics::buffer::address_type frame_address;
}; // struct grid_push

grid_pass::grid_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/passes/grid.slang", entry_points});

  auto info = graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {render_pass::hdr_format},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = graphics::cull_mode::none,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .color_blend_attachments = {graphics::blend_attachment{
      .enable = true,
      .source_color = graphics::blend_factor::source_alpha,
      .destination_color = graphics::blend_factor::one_minus_source_alpha,
      .color_operation = graphics::blend_operation::add,
      .source_alpha = graphics::blend_factor::one,
      .destination_alpha = graphics::blend_factor::one_minus_source_alpha,
      .alpha_operation = graphics::blend_operation::add
    }},
    .name = "Grid"
  };

  _pipeline = pipeline_cache.get(info);
}

auto grid_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  auto group = render_attachment_group{.extent = resources.extent};

  group.colors.push_back(color_attachment_slot{
    .image = resources.color_msaa,
    .access_mask = graphics::access::color_attachment_write | graphics::access::color_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .resolve_image = resources.color
  });

  group.depth = depth_attachment_slot{.image = resources.depth};

  builder.add_group(group);
}

auto grid_pass::should_execute(const render_context& context, std::uint32_t /*group*/) const -> bool {
  return context.show_grid && context.packet->camera.is_active;
}

auto grid_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  auto values = grid_push{context.frame_address};

  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &values, sizeof(values));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  context.command_buffer->draw(6u, 1u, 0u, 0u);
}

} // namespace sbx::render
