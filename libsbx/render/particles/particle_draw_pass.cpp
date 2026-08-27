// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/particles/particle_draw_pass.hpp>

#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

struct push_data {
  graphics::buffer::address_type frame;
  graphics::buffer::address_type particles;
  graphics::buffer::address_type alive_list;
  graphics::buffer::address_type emitters;
  std::uint32_t sampler_index;
}; // struct push_data

particle_draw_pass::particle_draw_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();

  const auto additive_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main", "additive_shading_policy"}
  };

  const auto& additive_shader = shader_cache.get({"shaders/particles/draw.slang", additive_entry_points});

  _additive_pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = additive_shader,
    .color_formats = {render_pass::hdr_format},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = graphics::cull_mode::none,
    .front_face = graphics::front_face::counter_clockwise,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .color_blend_attachments = {graphics::blend_attachment{
      .enable = true,
      .source_color = graphics::blend_factor::one,
      .destination_color = graphics::blend_factor::one,
      .color_operation = graphics::blend_operation::add,
      .source_alpha = graphics::blend_factor::one,
      .destination_alpha = graphics::blend_factor::one,
      .alpha_operation = graphics::blend_operation::add
    }},
    .name = "Particle Additive"
  });

  const auto alpha_entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main", "alpha_blend_shading_policy"}
  };

  const auto& alpha_shader = shader_cache.get({"shaders/particles/draw.slang", alpha_entry_points});

  _alpha_blend_pipeline = pipeline_cache.get(graphics::graphics_pipeline::create_info{
    .shader = alpha_shader,
    .color_formats = {render_pass::hdr_format, graphics::format::r16_sfloat},
    .depth_format = graphics::format::d32_sfloat,
    .cull_mode = graphics::cull_mode::none,
    .front_face = graphics::front_face::counter_clockwise,
    .depth_test = true,
    .depth_write = false,
    .depth_compare = graphics::compare_operation::less_or_equal,
    .samples = render_pass::sample_count,
    .color_blend_attachments = {
      graphics::blend_attachment{
        .enable = true,
        .source_color = graphics::blend_factor::one,
        .destination_color = graphics::blend_factor::one,
        .color_operation = graphics::blend_operation::add,
        .source_alpha = graphics::blend_factor::one,
        .destination_alpha = graphics::blend_factor::one,
        .alpha_operation = graphics::blend_operation::add
      },
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
    .name = "Particle Alpha Blend"
  });
}

auto particle_draw_pass::declare(graphics_pass_builder& builder, const graph_resources& resources) -> void {
  // Continuation writes, not fresh transitions — transparent_accumulate_pass (accumulator/
  // revealage) and opaque_pass (color) already wrote this frame; both groups blend onto existing
  // content (WRITE|READ), same reasoning as skybox_pass/grid_pass.
  auto additive = render_attachment_group{.extent = resources.extent};

  additive.colors.push_back(color_attachment_slot{
    .image = resources.color_msaa,
    .access_mask = graphics::access::color_attachment_write | graphics::access::color_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .resolve_image = resources.color
  });

  additive.depth = depth_attachment_slot{.image = resources.depth};

  builder.add_group(additive);

  auto alpha_blend = render_attachment_group{.extent = resources.extent};

  alpha_blend.colors.push_back(color_attachment_slot{
    .image = resources.accumulator_msaa,
    .access_mask = graphics::access::color_attachment_write | graphics::access::color_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .resolve_image = resources.accumulator
  });

  alpha_blend.colors.push_back(color_attachment_slot{
    .image = resources.revealage_msaa,
    .access_mask = graphics::access::color_attachment_write | graphics::access::color_attachment_read,
    .store_op = graphics::attachment_store_op::store,
    .resolve_image = resources.revealage
  });

  alpha_blend.depth = depth_attachment_slot{.image = resources.depth};

  builder.add_group(alpha_blend);
}

auto particle_draw_pass::should_execute(const render_context& context, std::uint32_t group) const -> bool {
  if (!context.packet->camera.is_active) {
    return false;
  }

  return group == additive_group ? context.particle_additive_draw_args.is_valid() : context.particle_alpha_draw_args.is_valid();
}

auto particle_draw_pass::execute(render_context& context, std::uint32_t group) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();

  bind_globals(context);

  if (group == additive_group) {
    context.command_buffer->bind_pipeline(*_additive_pipeline);

    auto data = push_data{
      context.frame_address,
      context.particle_additive_particles_address,
      context.particle_additive_alive_list_address,
      context.particle_additive_emitters_address,
      context.sampler_index
    };

    write_push_constants(context, data);

    auto& draw_args = registry.get<graphics::buffer>(context.particle_additive_draw_args);
    context.command_buffer->draw_indirect(draw_args, 0u, 1u);
  } else {
    context.command_buffer->bind_pipeline(*_alpha_blend_pipeline);

    auto data = push_data{
      context.frame_address,
      context.particle_alpha_particles_address,
      context.particle_alpha_alive_list_address,
      context.particle_alpha_emitters_address,
      context.sampler_index
    };

    write_push_constants(context, data);

    auto& draw_args = registry.get<graphics::buffer>(context.particle_alpha_draw_args);
    context.command_buffer->draw_indirect(draw_args, 0u, 1u);
  }
}

} // namespace sbx::render
