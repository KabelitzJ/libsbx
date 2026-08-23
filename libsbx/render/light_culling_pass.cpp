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

namespace sbx::render {

namespace {

// Mirrors shaders/pbr/cluster_data.slang's cluster_dim_x/y/z, which in turn must match
// render_module::cluster_dim_x/y/z (render_module.hpp owns the buffer sizing these dimensions
// imply — see its comment for why all three copies of these numbers have to agree by hand).
inline constexpr auto cluster_dim_x = std::uint32_t{16u};
inline constexpr auto cluster_dim_y = std::uint32_t{9u};
inline constexpr auto cluster_dim_z = std::uint32_t{24u};

inline constexpr auto threads_per_group = std::uint32_t{8u};

auto bind_compute_globals(graphics::command_buffer& command_buffer) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);
}

} // namespace

light_culling_pass::light_culling_pass() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto build_clusters_shader = shader_cache.get({"shaders/pbr/build_clusters.slang", entry_points});

  _build_clusters_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = build_clusters_shader,
    .name = "Build Clusters"
  });

  const auto cull_lights_shader = shader_cache.get({"shaders/pbr/cull_lights.slang", entry_points});

  _cull_lights_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = cull_lights_shader,
    .name = "Cull Lights"
  });
}

auto light_culling_pass::execute(render_context& context) -> void {
  if (!context.packet->camera.is_active) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& bindless_table = graphics_module.bindless_table();

  bind_compute_globals(*context.command_buffer);

  const auto& camera = context.packet->camera;
  const auto tan_half_fov_y = std::tan(math::angle{math::degree{camera.fov_degrees}}.to_radians() / 2.0f);
  const auto aspect = static_cast<std::float_t>(context.extent.x()) / static_cast<std::float_t>(context.extent.y());

  const auto groups_x = (cluster_dim_x + threads_per_group - 1u) / threads_per_group;
  const auto groups_y = (cluster_dim_y + threads_per_group - 1u) / threads_per_group;

  {
    struct push_data {
      graphics::buffer::address_type clusters;
      std::float_t tan_half_fov_y;
      std::float_t aspect;
      std::float_t near_plane;
      std::float_t far_plane;
    }; // struct push_data

    const auto push = push_data{
      context.cluster_aabb_address,
      tan_half_fov_y,
      aspect,
      camera.near_plane,
      camera.far_plane
    };

    auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
    std::memcpy(range.data(), &push, sizeof(push));

    context.command_buffer->bind_pipeline(*_build_clusters_pipeline);
    context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);
    context.command_buffer->dispatch(groups_x, groups_y, cluster_dim_z);
  }

  // build_clusters' writes to the AABB buffer must land before cull_lights reads them.
  auto aabb_ready = VkMemoryBarrier2{};
  aabb_ready.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  aabb_ready.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  aabb_ready.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  aabb_ready.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  aabb_ready.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
  context.command_buffer->memory_dependency(aabb_ready);

  {
    struct push_data {
      graphics::buffer::address_type frame;
      graphics::buffer::address_type clusters;
      graphics::buffer::address_type counter;
    }; // struct push_data

    const auto push = push_data{
      context.frame_address,
      context.cluster_aabb_address,
      context.cluster_counter_address
    };

    auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
    std::memcpy(range.data(), &push, sizeof(push));

    context.command_buffer->bind_pipeline(*_cull_lights_pipeline);
    context.command_buffer->push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);
    context.command_buffer->dispatch(groups_x, groups_y, cluster_dim_z);
  }

  // cull_lights' writes to the range/light-index buffers must land before opaque_pass and
  // transparent_accumulate_pass's vertex/fragment shaders read them.
  auto lights_ready = VkMemoryBarrier2{};
  lights_ready.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
  lights_ready.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  lights_ready.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  lights_ready.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  lights_ready.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
  context.command_buffer->memory_dependency(lights_ready);
}

} // namespace sbx::render
