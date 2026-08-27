// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/transparent_accumulate_pass.hpp>

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

transparent_accumulate_pass::transparent_accumulate_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main", "alpha_blend_shading_policy"}
  };

  const auto& shader = shader_cache.get({"shaders/pbr/geometry.slang", entry_points});

  const auto make = [&](graphics::cull_mode cull, const std::string& name) {
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
  };

  _pipelines[0] = make(graphics::cull_mode::back, "Transparent Accumulate");
  _pipelines[1] = make(graphics::cull_mode::none, "Transparent Accumulate Double-Sided");
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
  if (!context.packet->camera.is_active) {
    return;
  }

  bind_globals(context);
  submit_draw_commands(context, context.packet->transparent_commands, _pipelines);
}

} // namespace sbx::render
