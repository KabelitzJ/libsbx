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

auto shadow_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  const auto shadow_extent = math::vector2u{shadow_map_resolution, shadow_map_resolution};

  for (auto cascade = std::uint32_t{0u}; cascade < shadow_cascade_count; ++cascade) {
    auto group = render_attachment_group{.extent = shadow_extent};

    group.depth = depth_attachment_slot{
      .image = resources.shadow_maps[cascade],
      .access_mask = graphics::access::depth_stencil_attachment_write | graphics::access::depth_stencil_attachment_read,
      .store_op = graphics::attachment_store_op::store,
      .clear_value = graphics::depth_stencil_clear_value{1.0f, 0u}
    };

    const auto group_index = builder.add_group(group);

    // No consumer declares a shadow-map read (bindless sample) — must self-transition after
    // end_rendering() since Vulkan forbids transitioning an attachment inside its own render scope.
    builder.transitions_after(group_index, resources.shadow_maps[cascade], graphics::pipeline_stage::fragment_shader, graphics::access::shader_read, graphics::image_layout::shader_read_only_optimal);
  }
}

auto shadow_pass::should_execute(const render_context& context, std::uint32_t /*cascade*/) const -> bool {
  return context.has_shadow_caster;
}

auto shadow_pass::execute(render_context& context, std::uint32_t cascade) -> void {
  const auto shadow_extent = math::vector2u{shadow_map_resolution, shadow_map_resolution};

  bind_globals(context, shadow_extent);
  submit_draw_commands(context, context.packet->shadow_caster_commands, _pipelines, cascade);
}

} // namespace sbx::render
