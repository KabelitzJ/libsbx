// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/shadow/shadow_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

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

  const auto& shader = shader_cache.get({"engine://shaders/passes/shadow.slang", entry_points});

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
      .samples = graphics::samples::count_1,
      .name = name
    });
  };

  // cull_mode::back here (same winding culled as the color pass, not the reverse) -- a "cull
  // front" peter-panning trick only helps closed, roughly-convex casters that have a front/back
  // surface pair everywhere along their silhouette as seen from the light. Open meshes (terrain,
  // ground planes) have a single layer of front-facing triangles; culling front faces for a
  // caster like that discards almost the entire light-facing surface from the shadow map instead
  // of just avoiding self-acne on it. Slope-scaled bias (csm.slang) carries the acne-avoidance
  // burden instead, matching what the color pass already culls.
  _pipelines[0] = make(graphics::cull_mode::back, "Shadow Cascade");
  _pipelines[1] = make(graphics::cull_mode::none, "Shadow Cascade Double-Sided");
  _pipelines[2] = _pipelines[0]; // shading model doesn't affect depth-only output
  _pipelines[3] = _pipelines[1];
}

auto shadow_pass::_resolve_custom_pipeline(const std::string& shader_path, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline> {
  const auto entry_points = std::array<graphics::shader_compiler::entry_point_request, 2u>{
    graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_VERTEX_BIT, "depth_vertex_main"},
    graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_FRAGMENT_BIT, "depth_fragment_main"}
  };

  return resolve_custom_pipeline(shader_path, entry_points, graphics::graphics_pipeline::create_info{
    .color_formats = {},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = is_double_sided ? graphics::cull_mode::none : graphics::cull_mode::back,
    .front_face = graphics::front_face::counter_clockwise,
    .depth_test = true,
    .depth_write = true,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = graphics::samples::count_1,
  }, "Shadow Cascade");
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
  SBX_PROFILE_SCOPE("shadow_pass::execute");
  SBX_STATS_SCOPE("shadow_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "shadow_pass::execute");

  const auto shadow_extent = math::vector2u{shadow_map_resolution, shadow_map_resolution};

  bind_globals(context, shadow_extent);
  submit_draw_commands(context, context.packet->shadow_caster_commands, _pipelines, cascade, [this](const std::string& shader_path, bool is_double_sided) { return _resolve_custom_pipeline(shader_path, is_double_sided); });
}

} // namespace sbx::render
