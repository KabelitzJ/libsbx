// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/canvas_pass.hpp>

#include <array>
#include <cstring>
#include <span>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

#include <libsbx/canvas/canvas_module.hpp>

namespace sbx::render {

// New buffers are sized at this multiple of what's actually needed, so a slot that grows once
// tends not to grow again next frame -- same policy as debug_draw_pass.
constexpr auto growth_factor = 1.5f;

struct canvas_push {
  graphics::buffer::address_type vertex_address;
}; // struct canvas_push

canvas_pass::canvas_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();
  auto& surface = graphics_module.surface();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/passes/canvas.slang", entry_points});

  _pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {static_cast<graphics::format>(surface.format().format)},
    .cull_mode = graphics::cull_mode::none,
    .depth_test = false,
    .depth_write = false,
    .name = "Canvas"
  });
}

auto canvas_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  auto group = render_attachment_group{.extent = resources.extent};

  group.colors.push_back(color_attachment_slot{
    .image = resources.final_image,
    .store_op = graphics::attachment_store_op::store
  });

  builder.add_group(group);
}

auto canvas_pass::should_execute(const render_context& context, std::uint32_t /*group*/) const -> bool {
  auto& canvas_module = core::engine::get_module<canvas::canvas_module>();

  return context.packet->camera.is_active && !canvas_module.draw_list().vertices().empty();
}

auto canvas_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  auto& canvas_module = core::engine::get_module<canvas::canvas_module>();
  auto& draw_list = canvas_module.draw_list();

  const auto& vertices = draw_list.vertices();

  const auto slot = context.slot;

  if (vertices.size() > _capacities[slot]) {
    if (_buffers[slot].is_valid()) {
      registry.retire<graphics::buffer>(_buffers[slot], context.frame_index);
    }

    const auto new_capacity = static_cast<std::size_t>(static_cast<std::float_t>(vertices.size()) * growth_factor) + 1u;

    _buffers[slot] = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = new_capacity * sizeof(canvas::canvas_vertex),
      .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
      .memory = graphics::memory_usage::host_write,
      .name = "Canvas Vertices"
    });

    _capacities[slot] = new_capacity;
  }

  auto& buffer = registry.get<graphics::buffer>(_buffers[slot]);

  buffer.write(std::span{vertices});

  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  write_push_constants(context, canvas_push{buffer.address()});

  context.command_buffer->draw(static_cast<std::uint32_t>(vertices.size()), 1u, 0u, 0u);

  draw_list.clear();
}

} // namespace sbx::render
