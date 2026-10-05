// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/selection_outline_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

// Mirrors shaders/passes/jump_flood.slang's push_data.
struct jump_flood_push {
  std::uint32_t texture_index;
  std::uint32_t sampler_index;
  std::float_t texel_size_x;
  std::float_t texel_size_y;
  std::int32_t step;
  std::float_t color_r;
  std::float_t color_g;
  std::float_t color_b;
}; // struct jump_flood_push

namespace {

// Group indices, in declare() order.
inline constexpr auto mask_group = std::uint32_t{0u};
inline constexpr auto init_group = std::uint32_t{1u};
inline constexpr auto flood_group = std::uint32_t{2u};

auto srgb_to_linear(std::float_t value) -> std::float_t {
  return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

} // namespace

selection_outline_pass::selection_outline_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();
  auto& surface = graphics_module.surface();

  const auto& mask_shader = shader_cache.get({"engine://shaders/passes/selection_mask.slang", std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  }});

  const auto make_mask = [&](graphics::cull_mode cull, const std::string& name) {
    return pipeline_cache.get(graphics::graphics_pipeline::create_info{
      .shader = mask_shader,
      .color_formats = {graphics::format::r8g8b8a8_unorm},
      .depth_format = graphics::format::d32_sfloat,
      .cull_mode = cull,
      .front_face = graphics::front_face::counter_clockwise,
      .depth_test = true,
      .depth_write = true,
      .depth_compare = graphics::compare_operation::less_or_equal,
      .name = name
    });
  };

  _mask_pipelines[0] = make_mask(graphics::cull_mode::back, "Selection Mask");
  _mask_pipelines[1] = make_mask(graphics::cull_mode::none, "Selection Mask Double-Sided");
  _mask_pipelines[2] = _mask_pipelines[0]; // shading model doesn't affect the silhouette
  _mask_pipelines[3] = _mask_pipelines[1];

  const auto make_fullscreen = [&](std::string_view fragment_entry, graphics::format format, bool blend, const std::string& name) {
    const auto& shader = shader_cache.get({"engine://shaders/passes/jump_flood.slang", std::vector<graphics::shader_compiler::entry_point_request>{
      {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
      {VK_SHADER_STAGE_FRAGMENT_BIT, std::string{fragment_entry}}
    }});

    auto info = graphics::graphics_pipeline::create_info{
      .shader = shader,
      .color_formats = {format},
      .cull_mode = graphics::cull_mode::none,
      .name = name
    };

    if (blend) {
      info.color_blend_attachments = {graphics::blend_attachment{
        .enable = true,
        .source_color = graphics::blend_factor::source_alpha,
        .destination_color = graphics::blend_factor::one_minus_source_alpha,
        .color_operation = graphics::blend_operation::add,
        .source_alpha = graphics::blend_factor::one,
        .destination_alpha = graphics::blend_factor::one_minus_source_alpha,
        .alpha_operation = graphics::blend_operation::add
      }};
    }

    return pipeline_cache.get(info);
  };

  const auto final_format = static_cast<graphics::format>(surface.format().format);

  _init_pipeline = make_fullscreen("init_main", graphics::format::r32g32b32a32_sfloat, false, "Jump Flood Init");
  _flood_pipeline = make_fullscreen("flood_main", graphics::format::r32g32b32a32_sfloat, false, "Jump Flood");
  _composite_pipeline = make_fullscreen("composite_main", final_format, true, "Jump Flood Composite");

  if (final_format == graphics::format::b8g8r8a8_srgb || final_format == graphics::format::r8g8b8a8_srgb) {
    for (auto& channel : _outline_color) {
      channel = srgb_to_linear(channel);
    }
  }
}

auto selection_outline_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  const auto to_sampled = [&](std::uint32_t group_index, graphics::image_handle image) {
    // Sampled through bindless, so no declared read: a skipped group then doesn't force its clear to run.
    builder.transitions_after(group_index, image, graphics::pipeline_stage::fragment_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);
  };

  auto mask_group = render_attachment_group{.extent = resources.extent};

  mask_group.colors.push_back(color_attachment_slot{
    .image = resources.selection_mask,
    .clear_value = math::color{0.0f, 0.0f, 0.0f, 0.0f}
  });

  mask_group.depth = depth_attachment_slot{
    .image = resources.selection_depth,
    .access_mask = graphics::access::depth_stencil_attachment_write | graphics::access::depth_stencil_attachment_read,
    .store_op = graphics::attachment_store_op::dont_care,
    .clear_value = graphics::depth_stencil_clear_value{1.0f, 0u}
  };

  const auto mask_index = builder.add_group(mask_group);

  // Skinned selections draw from skin_pass's output.
  builder.reads_buffer(resources.skin_scratch_buffer, graphics::pipeline_stage::vertex_shader, graphics::access::shader_read, mask_index);
  to_sampled(mask_index, resources.selection_mask);

  for (auto i = std::size_t{0u}; i < resources.jump_flood.size(); ++i) {
    auto group = render_attachment_group{.extent = resources.extent};

    group.colors.push_back(color_attachment_slot{.image = resources.jump_flood[i]});

    to_sampled(builder.add_group(group), resources.jump_flood[i]);
  }

  auto composite_group = render_attachment_group{.extent = resources.extent};

  composite_group.colors.push_back(color_attachment_slot{
    .image = resources.final_image,
    .access_mask = graphics::access::color_attachment_write | graphics::access::color_attachment_read
  });

  builder.add_group(composite_group);
}

auto selection_outline_pass::should_execute(const render_context& context, std::uint32_t /*group*/) const -> bool {
  return context.packet->camera.is_active && !context.packet->selected_commands.empty();
}

auto selection_outline_pass::execute(render_context& context, std::uint32_t group) -> void {
  SBX_PROFILE_SCOPE("selection_outline_pass::execute");
  SBX_STATS_SCOPE("selection_outline_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "selection_outline_pass::execute");

  bind_globals(context);

  if (group == mask_group) {
    // Custom-shader materials outline their undisplaced mesh.
    submit_draw_commands(context, context.packet->selected_commands, _mask_pipelines, 0xFFFFFFFFu, [this](const std::string& /*shader_path*/, bool is_double_sided) {
      return _mask_pipelines[is_double_sided ? 1u : 0u];
    });

    return;
  }

  auto values = jump_flood_push{
    .sampler_index = context.clamp_sampler_index,
    .texel_size_x = 1.0f / static_cast<std::float_t>(context.extent.x()),
    .texel_size_y = 1.0f / static_cast<std::float_t>(context.extent.y()),
    .step = 1,
    .color_r = _outline_color[0],
    .color_g = _outline_color[1],
    .color_b = _outline_color[2]
  };

  switch (group) {
    case init_group: {
      context.command_buffer->bind_pipeline(*_init_pipeline);
      values.texture_index = context.selection_mask_index;
      break;
    }
    case flood_group: {
      context.command_buffer->bind_pipeline(*_flood_pipeline);
      values.texture_index = context.jump_flood_indices[0];
      break;
    }
    default: {
      context.command_buffer->bind_pipeline(*_composite_pipeline);
      values.texture_index = context.jump_flood_indices[1];
      break;
    }
  }

  write_push_constants(context, values);

  context.command_buffer->draw(3u, 1u, 0u, 0u);
}

} // namespace sbx::render
