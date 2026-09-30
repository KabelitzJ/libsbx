// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/passes/tonemap_pass.hpp>

#include <libsbx/utility/profiler.hpp>
#include <libsbx/utility/stats_registry.hpp>
#include <libsbx/graphics/profiler.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/memory/bytes.hpp>

#include <libsbx/graphics/frame_context.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/pipeline/shader.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

namespace sbx::render {

// Mirrors shaders/passes/tonemap.slang's push_data.
struct tonemap_push {
  graphics::buffer::address_type frame_address;  // fog: view, projection, camera position

  std::uint32_t color_index;
  std::uint32_t sampler_index;
  std::float_t exposure;
  std::uint32_t bloom_index;
  std::float_t bloom_intensity;

  // Depth of field: mode 0 = off, 1 = distance, 2 = screen band.
  std::uint32_t depth_index;
  std::uint32_t dof_mode;
  std::float_t dof_focus;      // focus distance (world units) or band centre (fraction of height)
  std::float_t dof_range;      // focus range or band height
  std::float_t dof_max_blur;   // fraction of screen height
  std::uint32_t dof_samples;
  std::float_t near_plane;
  std::float_t far_plane;
  std::float_t aspect;         // width / height

  // Colour grading: lut_index = texture::invalid_index for none.
  std::uint32_t lut_index;
  std::float_t lut_contribution;
  std::float_t contrast;
  std::float_t saturation;

  // Fog: density 0 = off.
  std::float_t fog_color_r;
  std::float_t fog_color_g;
  std::float_t fog_color_b;
  std::float_t fog_density;
  std::float_t fog_start;
  std::float_t fog_height_falloff;
  std::float_t fog_base_height;
  std::float_t fog_max_opacity;
  std::uint32_t fog_affects_sky;
}; // struct tonemap_push

static_assert(sizeof(tonemap_push) <= 128u, "Push constants must not exceed 128 bytes.");

tonemap_pass::tonemap_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& pipeline_cache = graphics_module.pipeline_cache();
  auto& surface = graphics_module.surface();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_VERTEX_BIT, "vertex_main"},
    {VK_SHADER_STAGE_FRAGMENT_BIT, "fragment_main"}
  };

  const auto& shader = shader_cache.get({"engine://shaders/passes/tonemap.slang", entry_points});

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

  // Depth of field's distance mode reads it; declared always so the barrier is there when it's on.
  builder.reads_image(resources.scene_depth, graphics::pipeline_stage::fragment_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);

  // bloom_pass declares this ready every frame, bloom enabled or not -- see its doc comment.
  builder.reads_image(resources.bloom_upsample, graphics::pipeline_stage::fragment_shader, graphics::access::shader_sampled_read, graphics::image_layout::shader_read_only_optimal);

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
  SBX_PROFILE_SCOPE("tonemap_pass::execute");
  SBX_STATS_SCOPE("tonemap_pass::execute");
  SBX_PROFILE_GPU_SCOPE((*context.command_buffer), "tonemap_pass::execute");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  if (!context.packet->camera.is_active) {
    return;
  }

  auto& bindless_table = graphics_module.bindless_table();

  bind_globals(context);

  context.command_buffer->bind_pipeline(*_pipeline);

  const auto& camera = context.packet->camera;
  const auto& post = camera.post_process;
  const auto& dof = post.depth_of_field;
  const auto& grading = post.color_grading;
  const auto band = dof.mode == scenes::post_process_settings::depth_of_field_settings::focus_mode::screen_band;

  const auto& fog = post.fog;

  auto values = tonemap_push{
    .frame_address = context.frame_address,
    .color_index = context.color_index,
    .sampler_index = context.sampler_index,
    .exposure = post.exposure,
    .bloom_index = context.bloom_upsample_index,
    .bloom_intensity = post.bloom.enabled ? post.bloom.intensity : 0.0f,
    .depth_index = context.scene_depth_index,
    .dof_mode = !dof.enabled || dof.max_blur <= 0.0f ? 0u : band ? 2u : 1u,
    .dof_focus = band ? dof.band_center : dof.focus_distance,
    .dof_range = band ? dof.band_height : dof.focus_range,
    .dof_max_blur = dof.max_blur,
    .dof_samples = std::clamp(dof.samples, 1u, 64u),
    .near_plane = camera.near_plane,
    .far_plane = camera.far_plane,
    .aspect = static_cast<std::float_t>(context.extent.x()) / static_cast<std::float_t>(std::max(context.extent.y(), 1u)),
    .lut_index = grading.lut.is_valid() ? grading.lut->index() : assets::texture::invalid_index,
    .lut_contribution = grading.lut_contribution,
    .contrast = grading.contrast,
    .saturation = grading.saturation,
    .fog_color_r = fog.color.r(),
    .fog_color_g = fog.color.g(),
    .fog_color_b = fog.color.b(),
    .fog_density = fog.enabled ? std::max(fog.density, 0.0f) : 0.0f,
    .fog_start = fog.start,
    .fog_height_falloff = std::max(fog.height_falloff, 0.0f),
    .fog_base_height = fog.base_height,
    .fog_max_opacity = std::clamp(fog.max_opacity, 0.0f, 1.0f),
    .fog_affects_sky = fog.affects_sky ? 1u : 0u
  };

  context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, memory::as_bytes(values));

  context.command_buffer->draw(3u, 1u, 0u, 0u);
}

} // namespace sbx::render
