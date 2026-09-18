// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/transparent_accumulate_pass.hpp>

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
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

auto transparent_accumulate_pass::_make_pipeline(memory::observer_ptr<const graphics::shader> shader, graphics::cull_mode cull, const std::string& name) -> memory::observer_ptr<graphics::graphics_pipeline> {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  auto info = graphics::graphics_pipeline::create_info{
    .shader = shader,
    .color_formats = {render_pass::hdr_format, graphics::format::r16_sfloat},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = cull,
    .front_face = graphics::front_face::counter_clockwise,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .name = name
  };

  info.color_blend_attachments = {
    // Accumulator: additive — sum of weight * premultiplied(color, alpha) across every
    // fragment that lands here, order-independent.
    graphics::blend_attachment{
      .enable = true,
      .source_color = graphics::blend_factor::one,
      .destination_color = graphics::blend_factor::one,
      .color_operation = graphics::blend_operation::add,
      .source_alpha = graphics::blend_factor::one,
      .destination_alpha = graphics::blend_factor::one,
      .alpha_operation = graphics::blend_operation::add
    },
    // Revealage: multiplicative — dst *= (1 - alpha), the classic
    // glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR) McGuire/Bavoil recipe.
    graphics::blend_attachment{
      .enable = true,
      .source_color = graphics::blend_factor::zero,
      .destination_color = graphics::blend_factor::one_minus_source_color,
      .color_operation = graphics::blend_operation::add,
      .source_alpha = graphics::blend_factor::zero,
      .destination_alpha = graphics::blend_factor::one_minus_source_color,
      .alpha_operation = graphics::blend_operation::add
    }
  };

  return pipeline_cache.get(info);
}

auto transparent_accumulate_pass::_resolve_graph_pipeline(const assets::shader_graph_handle& graph, bool is_double_sided) -> memory::observer_ptr<graphics::graphics_pipeline> {
  const auto entry_points = std::array<graphics::shader_compiler::entry_point_request, 2u>{
    graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    graphics::shader_compiler::entry_point_request{VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main", "alpha_blend_shading_policy"}
  };

  return resolve_graph_pipeline(graph, entry_points, graphics::graphics_pipeline::create_info{
    .color_formats = {render_pass::hdr_format, graphics::format::r16_sfloat},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = is_double_sided ? graphics::cull_mode::none : graphics::cull_mode::back,
    .front_face = graphics::front_face::counter_clockwise,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .color_blend_attachments = {
      // Accumulator: additive — sum of weight * premultiplied(color, alpha) across every
      // fragment that lands here, order-independent.
      graphics::blend_attachment{
        .enable = true,
        .source_color = graphics::blend_factor::one,
        .destination_color = graphics::blend_factor::one,
        .color_operation = graphics::blend_operation::add,
        .source_alpha = graphics::blend_factor::one,
        .destination_alpha = graphics::blend_factor::one,
        .alpha_operation = graphics::blend_operation::add
      },
      // Revealage: multiplicative — dst *= (1 - alpha), the classic
      // glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR) McGuire/Bavoil recipe.
      graphics::blend_attachment{
        .enable = true,
        .source_color = graphics::blend_factor::zero,
        .destination_color = graphics::blend_factor::one_minus_source_color,
        .color_operation = graphics::blend_operation::add,
        .source_alpha = graphics::blend_factor::zero,
        .destination_alpha = graphics::blend_factor::one_minus_source_color,
        .alpha_operation = graphics::blend_operation::add
      }
    },
  }, "Transparent Accumulate");
}

transparent_accumulate_pass::transparent_accumulate_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();

  const auto pbr_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main_pbr", "alpha_blend_shading_policy"}
  };

  const auto unlit_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main_unlit", "alpha_blend_shading_policy"}
  };

  const auto& pbr_shader = shader_cache.get({"engine://shaders/pbr/geometry.slang", pbr_entry_points});
  const auto& unlit_shader = shader_cache.get({"engine://shaders/pbr/geometry.slang", unlit_entry_points});

  _pipelines[0] = _make_pipeline(pbr_shader, graphics::cull_mode::back, "Transparent Accumulate");
  _pipelines[1] = _make_pipeline(pbr_shader, graphics::cull_mode::none, "Transparent Accumulate Double-Sided");
  _pipelines[2] = _make_pipeline(unlit_shader, graphics::cull_mode::back, "Transparent Accumulate Unlit");
  _pipelines[3] = _make_pipeline(unlit_shader, graphics::cull_mode::none, "Transparent Accumulate Unlit Double-Sided");
}

auto transparent_accumulate_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  auto group = render_attachment_group{.extent = resources.extent};

  group.colors.push_back(color_attachment_slot{
    .image = resources.accumulator_msaa,
    .store_op = graphics::attachment_store_op::store,
    .clear_value = math::color{0.0f, 0.0f, 0.0f, 0.0f},
    .resolve_image = resources.accumulator
  });

  group.colors.push_back(color_attachment_slot{
    .image = resources.revealage_msaa,
    .store_op = graphics::attachment_store_op::store,
    .clear_value = math::color{1.0f, 0.0f, 0.0f, 0.0f},
    .resolve_image = resources.revealage
  });

  group.depth = depth_attachment_slot{.image = resources.depth};

  builder.add_group(group);
}

auto transparent_accumulate_pass::execute(render_context& context, std::uint32_t /*group*/) -> void {
  SBX_PROFILE_SCOPE("transparent_accumulate_pass::execute");
  SBX_STATS_SCOPE("transparent_accumulate_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "transparent_accumulate_pass::execute");

  if (!context.packet->camera.is_active) {
    return;
  }

  bind_globals(context);
  submit_draw_commands(context, context.packet->transparent_commands, _pipelines, 0xFFFFFFFFu, [this](const assets::shader_graph_handle& graph, bool is_double_sided) { return _resolve_graph_pipeline(graph, is_double_sided); });
}

} // namespace sbx::render
