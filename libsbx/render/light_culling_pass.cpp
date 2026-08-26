// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/render/light_culling_pass.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/math/angle.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>

#include <libsbx/render/render_module.hpp>

namespace sbx::render {

inline constexpr auto threads_per_group = std::uint32_t{8u};

light_culling_pass::light_culling_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto build_clusters_shader = shader_cache.get({"shaders/clusters/build_clusters.slang", entry_points});

  _build_clusters_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = build_clusters_shader,
    .name = "Build Clusters"
  });

  const auto cull_lights_shader = shader_cache.get({"shaders/clusters/cull_lights.slang", entry_points});

  _cull_lights_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = cull_lights_shader,
    .name = "Cull Lights"
  });
}

struct build_cluster_push_data {
  graphics::buffer::address_type clusters;
  std::float_t tan_half_fov_y;
  std::float_t aspect;
  std::float_t near_plane;
  std::float_t far_plane;
}; // struct build_cluster_push_data

struct cull_lights_push_data {
  graphics::buffer::address_type frame;
  graphics::buffer::address_type clusters;
  graphics::buffer::address_type counter;
}; // struct cull_lights_push_data

auto light_culling_pass::execute(render_context& context) -> void {
  if (!context.packet->camera.is_active) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  bind_compute_globals(context);

  const auto& camera = context.packet->camera;
  const auto tan_half_fov_y = std::tan(math::angle{math::degree{camera.fov_degrees}}.to_radians() / 2.0f);
  const auto aspect = static_cast<std::float_t>(context.extent.x()) / static_cast<std::float_t>(context.extent.y());

  const auto groups_x = (cluster_dimensions.x() + threads_per_group - 1u) / threads_per_group;
  const auto groups_y = (cluster_dimensions.y() + threads_per_group - 1u) / threads_per_group;

  context.command_buffer->bind_pipeline(*_build_clusters_pipeline);

  const auto build_cluster_data = build_cluster_push_data{
    context.cluster_aabb_address,
    tan_half_fov_y,
    aspect,
    camera.near_plane,
    camera.far_plane
  };

  write_push_constants(context, build_cluster_data);
  
  context.command_buffer->dispatch(groups_x, groups_y, cluster_dimensions.z());

  // build_clusters' writes to the AABB buffer must land before cull_lights reads them.
  auto aabb_ready = VkMemoryBarrier2{};
  aabb_ready.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  aabb_ready.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  aabb_ready.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  aabb_ready.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  aabb_ready.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
  context.command_buffer->memory_dependency(aabb_ready);

  context.command_buffer->bind_pipeline(*_cull_lights_pipeline);

  const auto cull_lights_data = cull_lights_push_data{
    context.frame_address,
    context.cluster_aabb_address,
    context.cluster_counter_address
  };

  write_push_constants(context, cull_lights_data);

  context.command_buffer->dispatch(groups_x, groups_y, cluster_dimensions.z());

  auto lights_ready = VkMemoryBarrier2{};
  lights_ready.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  lights_ready.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  lights_ready.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  lights_ready.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  lights_ready.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
  context.command_buffer->memory_dependency(lights_ready);
}

} // namespace sbx::render
