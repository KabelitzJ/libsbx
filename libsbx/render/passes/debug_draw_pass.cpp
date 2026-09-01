// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/debug_draw_pass.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
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

#include <libsbx/render/scene_renderer_module.hpp>
#include <libsbx/render/debug/debug_draw.hpp>

namespace sbx::render {

// New buffers are sized at this multiple of what's actually needed, so a slot that grows once
// tends not to grow again next frame.
constexpr auto growth_factor = 1.5f;

struct debug_draw_push {
  graphics::buffer::address_type frame_address;
  graphics::buffer::address_type vertex_address;
}; // struct debug_draw_push

debug_draw_pass::debug_draw_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/passes/debug_draw.slang", entry_points});

  auto info = graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {render_pass::hdr_format},
    .depth_format = graphics::format::d32_sfloat,
    .topology = graphics::primitive_topology::line_list,
    .cull_mode = graphics::cull_mode::none,
    .line_width = 3.0f,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .name = "Debug Draw"
  };

  _pipeline = pipeline_cache.get(info);
}

auto debug_draw_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
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

auto debug_draw_pass::should_execute(const render_context& context, std::uint32_t /*group*/) const -> bool {
  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();

  return context.packet->camera.is_active && !scene_renderer_module.debug_draw().vertices().empty();
}

auto debug_draw_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();
  auto& registry = graphics_module.resource_registry();

  auto& scene_renderer_module = core::engine::get_module<render::scene_renderer_module>();
  auto& debug_draw = scene_renderer_module.debug_draw();

  const auto& vertices = debug_draw.vertices();

  const auto slot = context.slot;

  if (vertices.size() > _capacities[slot]) {
    if (_buffers[slot].is_valid()) {
      registry.retire<graphics::buffer>(_buffers[slot], context.frame_index);
    }

    const auto new_capacity = static_cast<std::size_t>(static_cast<std::float_t>(vertices.size()) * growth_factor) + 1u;

    _buffers[slot] = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = static_cast<graphics::buffer::size_type>(new_capacity * sizeof(render::debug_vertex)),
      .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
      .memory = graphics::memory_usage::host_write,
      .name = "Debug Draw Vertices"
    });

    _capacities[slot] = new_capacity;
  }

  auto& buffer = registry.get<graphics::buffer>(_buffers[slot]);

  buffer.write(std::span{vertices});

  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  auto values = debug_draw_push{context.frame_address, buffer.address()};

  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &values, sizeof(values));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  context.command_buffer->draw(static_cast<std::uint32_t>(vertices.size()), 1u, 0u, 0u);

  debug_draw.clear();
}

} // namespace sbx::render
