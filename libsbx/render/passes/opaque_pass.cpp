// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/opaque_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/graphics/profiler.hpp>

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

auto opaque_pass::_make_pipeline(memory::observer_ptr<const graphics::shader> shader, graphics::cull_mode cull, const std::string& name) -> memory::observer_ptr<graphics::graphics_pipeline> {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  return pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {render_pass::hdr_format},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = cull,
    .front_face = graphics::front_face::counter_clockwise,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .specialization_constants = {{0u, shadow_pcf_quality}},
    .name = name
  });
}

auto opaque_pass::_resolve_graph_pipeline(const assets::shader_graph_handle& graph, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline> {
  const auto entry_points = std::array<graphics::shader_compiler::entry_point_request, 2u>{
    graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main", "opaque_shading_policy"}
  };

  return resolve_graph_pipeline(graph, entry_points, graphics::graphics_pipeline::create_info{
    .color_formats = {render_pass::hdr_format},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = is_double_sided ? graphics::cull_mode::none : graphics::cull_mode::back,
    .front_face = graphics::front_face::counter_clockwise,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .specialization_constants = {{0u, shadow_pcf_quality}},
  }, "Mesh Opaque");
}

opaque_pass::opaque_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();

  const auto pbr_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main_pbr", "opaque_shading_policy"}
  };

  const auto unlit_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main_unlit", "opaque_shading_policy"}
  };

  const auto& pbr_shader = shader_cache.get({"engine://shaders/pbr/geometry.slang", pbr_entry_points});
  const auto& unlit_shader = shader_cache.get({"engine://shaders/pbr/geometry.slang", unlit_entry_points});

  _pipelines[0] = _make_pipeline(pbr_shader, graphics::cull_mode::back, "Mesh Opaque");
  _pipelines[1] = _make_pipeline(pbr_shader, graphics::cull_mode::none, "Mesh Opaque Double-Sided");
  _pipelines[2] = _make_pipeline(unlit_shader, graphics::cull_mode::back, "Mesh Opaque Unlit");
  _pipelines[3] = _make_pipeline(unlit_shader, graphics::cull_mode::none, "Mesh Opaque Unlit Double-Sided");
}

auto opaque_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  auto group = render_attachment_group{.extent = resources.extent};

  group.colors.push_back(color_attachment_slot{
    .image = resources.color_msaa,
    .store_op = graphics::attachment_store_op::store,
    .clear_value = math::color{0.05f, 0.05f, 0.08f, 1.0f},
    .resolve_image = resources.color
  });

  // Owns the depth "first reader" barrier: depth_pre_pass wrote it (last_was_write), so the
  // compiler emits a WAR barrier here automatically even though this slot only ever reads it.
  group.depth = depth_attachment_slot{.image = resources.depth};

  builder.add_group(group);
}

auto opaque_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  SBX_PROFILE_SCOPE("opaque_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "opaque_pass::execute");

  if (!context.packet->camera.is_active) {
    return;
  }

  bind_globals(context);
  submit_draw_commands_indirect(context, context.packet->opaque_commands, _pipelines, [this](const assets::shader_graph_handle& graph, bool is_double_sided) { return _resolve_graph_pipeline(graph, is_double_sided); });
}

} // namespace sbx::render
