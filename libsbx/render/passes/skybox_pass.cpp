// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/skybox_pass.hpp>

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

struct skybox_push {
  math::matrix4x4 inverse_view_projection;
  math::vector4 camera_position;
  std::uint32_t environment_index;
  std::uint32_t sampler_index;
  std::float_t environment_intensity;
}; // struct skybox_push

skybox_pass::skybox_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/passes/skybox.slang", entry_points});

  _pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {render_pass::hdr_format},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = graphics::cull_mode::none,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .name = "Skybox"
  });
}

auto skybox_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  auto group = render_attachment_group{.extent = resources.extent};

  // Continuation write, not a fresh transition — opaque_pass already wrote color_msaa/color this
  // frame.
  group.colors.push_back(color_attachment_slot{
    .image = resources.color_msaa,
    .access_mask = graphics::access::color_attachment_write | graphics::access::color_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .resolve_image = resources.color
  });

  // No depth barrier needed: depth has stayed in depth_attachment_optimal since opaque_pass, and
  // this is a read-only use — the compiler elides it automatically (read-after-read).
  group.depth = depth_attachment_slot{.image = resources.depth};

  builder.add_group(group);
}

auto skybox_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& bindless_table = graphics_module.bindless_table();

  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  auto values = skybox_push{context.inverse_view_projection, math::vector4{context.packet->camera.position, 1.0f}, context.environment_index, context.sampler_index, context.environment_intensity};

  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &values, sizeof(values));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  context.command_buffer->draw(3u, 1u, 0u, 0u);
}

} // namespace sbx::render
