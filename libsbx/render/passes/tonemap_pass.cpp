// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/tonemap_pass.hpp>

#include <array>
#include <cmath>
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

struct tonemap_push {
  std::uint32_t color_index;
  std::uint32_t sampler_index;
  std::float_t exposure;
}; // struct tonemap_push

tonemap_pass::tonemap_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();
  auto& surface = graphics_module.surface();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"shaders/passes/tonemap.slang", entry_points});

  _pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {static_cast<graphics::format>(surface.format().format)},
    .cull_mode = graphics::cull_mode::none,
    .depth_test = false,
    .depth_write = false,
    .name = "Tonemap"
  });
}

auto tonemap_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  // HDR target: geometry's writes -> this pass's sampled reads.
  builder.reads_image(resources.color, graphics::pipeline_stage::fragment_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);

  auto group = render_attachment_group{.extent = resources.extent};

  // final_image's first (and only, this compile) touch, so the compiler clears it — a fullscreen
  // triangle overwrites every pixel regardless, so the clear's contents never actually show.
  group.colors.push_back(color_attachment_slot{
    .image = resources.final_image,
    .store_op = graphics::attachment_store_op::store
  });

  builder.add_group(group);
}

auto tonemap_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& bindless_table = graphics_module.bindless_table();

  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  auto values = tonemap_push{context.color_index, context.sampler_index, context.packet->camera.exposure};
  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &values, sizeof(values));

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  context.command_buffer->draw(3u, 1u, 0u, 0u);
}

} // namespace sbx::render
