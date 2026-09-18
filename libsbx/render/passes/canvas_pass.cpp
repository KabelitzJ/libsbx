// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/canvas_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

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

constexpr auto growth_factor = 1.5f;

struct canvas_push {
  graphics::buffer::address_type vertex_address;
  std::uint32_t sampler_index;
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

  const auto text_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main_text"}
  };

  const auto& shader = shader_cache.get({"engine://shaders/passes/canvas.slang", entry_points});
  const auto& text_shader = shader_cache.get({"engine://shaders/passes/canvas.slang", text_entry_points});

  const auto blend = graphics::blend_attachment{
    .enable = true,
    .source_color = graphics::blend_factor::source_alpha,
    .destination_color = graphics::blend_factor::one_minus_source_alpha,
    .color_operation = graphics::blend_operation::add,
    .source_alpha = graphics::blend_factor::one,
    .destination_alpha = graphics::blend_factor::one_minus_source_alpha,
    .alpha_operation = graphics::blend_operation::add,
  };

  _pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {static_cast<graphics::format>(surface.format().format)},
    .cull_mode = graphics::cull_mode::none,
    .depth_test = false,
    .depth_write = false,
    .color_blend_attachments = {blend},
    .name = "Canvas"
  });

  _text_pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = text_shader,
    .color_formats = {static_cast<graphics::format>(surface.format().format)},
    .cull_mode = graphics::cull_mode::none,
    .depth_test = false,
    .depth_write = false,
    .color_blend_attachments = {blend},
    .name = "Canvas Text"
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

  auto& draw_list = canvas_module.draw_list();

  return context.packet->camera.is_active && (!draw_list.vertices().empty() || !draw_list.glyph_vertices().empty());
}

auto canvas_pass::_draw(render_context& context, graphics::graphics_pipeline& pipeline, std::array<graphics::buffer_handle, graphics::swapchain::max_frames_in_flight>& buffers, std::array<std::size_t, graphics::swapchain::max_frames_in_flight>& capacities, const std::vector<canvas::canvas_vertex>& vertices) -> void {
  if (vertices.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  const auto slot = context.slot;

  if (vertices.size() > capacities[slot]) {
    if (buffers[slot].is_valid()) {
      registry.retire<graphics::buffer>(buffers[slot], context.frame_index);
    }

    const auto new_capacity = static_cast<std::size_t>(static_cast<std::float_t>(vertices.size()) * growth_factor) + 1u;

    buffers[slot] = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
      .size = new_capacity * sizeof(canvas::canvas_vertex),
      .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
      .memory = graphics::memory_usage::host_write,
      .name = "Canvas Vertices"
    });

    capacities[slot] = new_capacity;
  }

  auto& buffer = registry.get<graphics::buffer>(buffers[slot]);

  buffer.write(std::span{vertices});

  context.command_buffer->bind_pipeline(pipeline);

  write_push_constants(context, canvas_push{buffer.address(), context.sampler_index});

  context.command_buffer->draw(static_cast<std::uint32_t>(vertices.size()), 1u, 0u, 0u);
}

auto canvas_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  SBX_PROFILE_SCOPE("canvas_pass::execute");
  SBX_STATS_SCOPE("canvas_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "canvas_pass::execute");

  auto& canvas_module = core::engine::get_module<canvas::canvas_module>();
  auto& draw_list = canvas_module.draw_list();

  bind_globals(context);

  _draw(context, *_pipeline, _buffers, _capacities, draw_list.vertices());
  _draw(context, *_text_pipeline, _glyph_buffers, _glyph_capacities, draw_list.glyph_vertices());

  draw_list.clear();
}

} // namespace sbx::render
