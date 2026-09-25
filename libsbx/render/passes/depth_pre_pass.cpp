// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/depth_pre_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
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

  const auto& shader = shader_cache.get({"engine://shaders/passes/depth_pre.slang", entry_points});

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
  _pipelines[2] = _pipelines[0]; // shading model doesn't affect depth-only output
  _pipelines[3] = _pipelines[1];

  // Queried once here rather than every declare() -- a device's supported resolve modes are fixed
  // for its lifetime. Only sample_zero is Vulkan-spec-guaranteed for a depth resolve; min/max/average
  // are all equally hardware-optional (VkPhysicalDeviceDepthStencilResolveProperties), so min is
  // used only when this specific device actually reports it.
  auto resolve_properties = VkPhysicalDeviceDepthStencilResolveProperties{};
  resolve_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES;

  auto properties = VkPhysicalDeviceProperties2{};
  properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  properties.pNext = &resolve_properties;

  vkGetPhysicalDeviceProperties2(graphics_module.physical_device().handle(), &properties);

  _scene_depth_resolve_mode = (resolve_properties.supportedDepthResolveModes & VK_RESOLVE_MODE_MIN_BIT) != 0u
    ? graphics::resolve_mode::min
    : graphics::resolve_mode::sample_zero;
}

auto depth_pre_pass::_resolve_custom_pipeline(const std::string& shader_path, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline> {
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
    .samples = render_pass::sample_count,
  }, "Depth Pre");
}

auto depth_pre_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  auto group = render_attachment_group{.extent = resources.extent};

  group.depth = depth_attachment_slot{
    .image = resources.depth,
    .access_mask = graphics::access::depth_stencil_attachment_write | graphics::access::depth_stencil_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .clear_value = graphics::depth_stencil_clear_value{1.0f, 0u},
    .resolve_image = resources.scene_depth,
    .resolve_mode = _scene_depth_resolve_mode
  };

  const auto group_index = builder.add_group(group);

  // No consumer declares a scene-depth read (bindless sample, by whichever arbitrary shader-graph
  // material's Scene Depth node happens to use it) -- must self-transition after end_rendering(),
  // same reasoning shadow_pass's own identical comment on its own private attachment has.
  builder.transitions_after(group_index, resources.scene_depth, graphics::pipeline_stage::fragment_shader, graphics::access::shader_read, graphics::image_layout::shader_read_only_optimal);
}

auto depth_pre_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  SBX_PROFILE_SCOPE("depth_pre_pass::execute");
  SBX_STATS_SCOPE("depth_pre_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "depth_pre_pass::execute");

  if (!context.packet->camera.is_active) {
    return;
  }

  bind_globals(context);
  submit_draw_commands_indirect(context, context.packet->opaque_commands, _pipelines, [this](const std::string& shader_path, bool is_double_sided) { return _resolve_custom_pipeline(shader_path, is_double_sided); });
}

} // namespace sbx::render
