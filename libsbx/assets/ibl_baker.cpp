// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/assets/ibl_baker.hpp>

#include <algorithm>
#include <array>
#include <cstring>

#include <vulkan/vulkan.h>

#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/bindless_table.hpp>
#include <libsbx/graphics/resources/sampler.hpp>
#include <libsbx/graphics/resources/buffer.hpp>
#include <libsbx/graphics/pipeline/shader_cache.hpp>
#include <libsbx/graphics/pipeline/shader_compiler.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline_cache.hpp>

namespace sbx::assets {

auto ibl_baker::_ensure_brdf_lut(graphics::command_buffer& command_buffer) -> void {
  if (_brdf_lut_index != environment_map::invalid_index) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  constexpr auto threads_per_group = std::uint32_t{8u};

  _brdf_lut_image = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{brdf_lut_size, brdf_lut_size, 1u},
    .format = graphics::format::r16g16_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .concurrent_sharing = true,
    .name = "IBL BRDF LUT"
  });

  auto& brdf_lut = registry.get<graphics::image>(_brdf_lut_image);

  auto to_general = graphics::command_buffer::image_transition_data{};
  to_general.image = brdf_lut.handle();
  to_general.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
  to_general.src_access_mask = VK_ACCESS_2_NONE;
  to_general.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  to_general.dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
  to_general.old_layout = graphics::image_layout::undefined;
  to_general.new_layout = graphics::image_layout::general;
  to_general.aspect_mask = brdf_lut.aspect();
  command_buffer.transition_image_layout(to_general);

  // Leaked forever, on purpose: runs once per program lifetime, so recovering this bindless slot
  // isn't worth the bookkeeping (unlike bake_environment, which runs per load and does clean up).
  const auto output_index = bindless_table.register_storage_image(brdf_lut.view());

  bindless_table.flush_writes();

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto shader = shader_cache.get({"shaders/ibl/brdf_lut.slang", entry_points});

  auto pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{
    .shader = shader,
    .name = "IBL BRDF LUT"
  });

  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  command_buffer.bind_pipeline(*pipeline);

  struct push_data {
    std::uint32_t output_index;
  }; // struct push_data

  auto push = push_data{output_index};

  auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
  std::memcpy(range.data(), &push, sizeof(push));
  command_buffer.push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

  const auto groups = (brdf_lut_size + threads_per_group - 1u) / threads_per_group;
  command_buffer.dispatch(groups, groups, 1u);

  auto to_read = graphics::command_buffer::image_transition_data{};
  to_read.image = brdf_lut.handle();
  to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  to_read.src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
  to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
  to_read.old_layout = graphics::image_layout::general;
  to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
  to_read.aspect_mask = brdf_lut.aspect();
  command_buffer.transition_image_layout(to_read);

  _brdf_lut_index = bindless_table.reserve_sampled_image();
  bindless_table.write_sampled_image(_brdf_lut_index, brdf_lut.view());
  bindless_table.flush_writes();
}

auto ibl_baker::bake_environment(environment_map& record, const std::vector<std::byte>& pixels, std::uint32_t width, std::uint32_t height) -> void {
  constexpr auto threads_per_group = std::uint32_t{8u};

  // Bindless indices that must stay untouched until this command buffer finishes on the GPU:
  // vkUpdateDescriptorSets applies immediately on the host, so reusing a slot before submit_idle()
  // returns could make an earlier dispatch in this buffer see a later write.
  struct transient_storage {
    std::uint32_t index;
    VkImageView view;
  }; // struct transient_storage

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();
  auto& bindless_table = graphics_module.bindless_table();
  auto& shader_cache = graphics_module.shader_cache();
  auto& compute_pipeline_cache = graphics_module.compute_pipeline_cache();

  const auto sampler_index = bindless_table.sampler_index(graphics::sampler::create_info{});

  auto command_buffer = graphics::command_buffer{graphics::queue::type::compute, true};

  auto pending_storage = std::vector<transient_storage>{};
  auto pending_sampled = std::vector<std::uint32_t>{};

  const auto entry_points = std::vector<graphics::shader_compiler::entry_point_request>{
    {VK_SHADER_STAGE_COMPUTE_BIT, "compute_main"}
  };

  const auto descriptor_set = bindless_table.descriptor_set();
  vkCmdBindDescriptorSets(command_buffer.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, bindless_table.pipeline_layout(), 0u, 1u, &descriptor_set, 0u, nullptr);

  // --- Radiance: upload the equirectangular source. Persistent — the skybox pass samples this
  // same index directly, so it isn't scratch data like the cube derived from it below. ---

  const auto radiance_handle = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{width, height, 1u},
    .format = graphics::format::r32g32b32a32_sfloat,
    .usage = graphics::image_usage::transfer_destination | graphics::image_usage::sampled,
    .concurrent_sharing = true,
    .name = "Environment Radiance"
  });

  auto& radiance = registry.get<graphics::image>(radiance_handle);

  auto staging = graphics::buffer{graphics::buffer::create_info{
    .size = static_cast<graphics::buffer::size_type>(pixels.size()),
    .usage = graphics::buffer_usage::transfer_source,
    .memory = graphics::memory_usage::host_write,
    .name = "Environment Staging"
  }};

  staging.write(pixels.data(), static_cast<graphics::buffer::size_type>(pixels.size()));

  {
    auto to_transfer = graphics::command_buffer::image_transition_data{};
    to_transfer.image = radiance.handle();
    to_transfer.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    to_transfer.src_access_mask = VK_ACCESS_2_NONE;
    to_transfer.dst_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_transfer.dst_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_transfer.old_layout = graphics::image_layout::undefined;
    to_transfer.new_layout = graphics::image_layout::transfer_destination_optimal;
    to_transfer.aspect_mask = radiance.aspect();
    command_buffer.transition_image_layout(to_transfer);

    auto region = VkBufferImageCopy{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1u;
    region.imageExtent = VkExtent3D{width, height, 1u};
    vkCmdCopyBufferToImage(command_buffer.handle(), staging.handle(), radiance.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);

    auto to_read = graphics::command_buffer::image_transition_data{};
    to_read.image = radiance.handle();
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_read.src_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = graphics::image_layout::transfer_destination_optimal;
    to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
    to_read.aspect_mask = radiance.aspect();
    command_buffer.transition_image_layout(to_read);
  }

  const auto radiance_index = bindless_table.register_sampled_image(radiance.view());

  // --- Equirect -> cubemap: transient single-mip scratch input for the convolutions below; never
  // touched by the graphics queue, so exclusive sharing (default) is fine. Both convolution shaders
  // always sample mip 0, so a mip chain here would be wasted work. ---

  auto radiance_cube = graphics::image{graphics::image::create_info{
    .extent = math::vector3u{radiance_cube_size, radiance_cube_size, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .array_layers = 6u,
    .view_type = graphics::image_view_type::cube,
    .name = "Environment Radiance Cube (scratch)"
  }};

  const auto equirect_to_cube_shader = shader_cache.get({"shaders/ibl/equirect_to_cubemap.slang", entry_points});
  auto equirect_to_cube_pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{.shader = equirect_to_cube_shader, .name = "IBL Equirect To Cubemap"});

  {
    auto to_general = graphics::command_buffer::image_transition_data{};
    to_general.image = radiance_cube.handle();
    to_general.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    to_general.src_access_mask = VK_ACCESS_2_NONE;
    to_general.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_general.dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_general.old_layout = graphics::image_layout::undefined;
    to_general.new_layout = graphics::image_layout::general;
    to_general.aspect_mask = radiance_cube.aspect();
    to_general.layer_count = 6u;
    command_buffer.transition_image_layout(to_general);

    const auto mip0_view = radiance_cube.create_view(graphics::image_view_type::two_dimensional_array, 0u, 1u, 0u, 6u);
    const auto output_index = bindless_table.register_storage_cube(mip0_view);
    pending_storage.push_back({output_index, mip0_view});

    bindless_table.flush_writes();

    struct push_data {
      std::uint32_t source_index;
      std::uint32_t sampler_index;
      std::uint32_t output_index;
    }; // struct push_data

    auto push = push_data{radiance_index, sampler_index, output_index};

    auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
    std::memcpy(range.data(), &push, sizeof(push));

    command_buffer.bind_pipeline(*equirect_to_cube_pipeline);
    command_buffer.push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

    const auto groups = (radiance_cube_size + threads_per_group - 1u) / threads_per_group;
    command_buffer.dispatch(groups, groups, 6u);
  }

  {
    auto to_read = graphics::command_buffer::image_transition_data{};
    to_read.image = radiance_cube.handle();
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = graphics::image_layout::general;
    to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
    to_read.aspect_mask = radiance_cube.aspect();
    to_read.layer_count = 6u;
    command_buffer.transition_image_layout(to_read);
  }

  const auto radiance_cube_sampled_index = bindless_table.register_sampled_cube(radiance_cube.view());
  pending_sampled.push_back(radiance_cube_sampled_index);

  bindless_table.flush_writes();

  // --- Irradiance: single-mip cube, cosine-weighted hemisphere convolution ---

  const auto irradiance_handle = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{irradiance_cube_size, irradiance_cube_size, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .array_layers = 6u,
    .view_type = graphics::image_view_type::cube,
    .concurrent_sharing = true,
    .name = "IBL Irradiance"
  });

  auto& irradiance = registry.get<graphics::image>(irradiance_handle);

  {
    auto to_general = graphics::command_buffer::image_transition_data{};
    to_general.image = irradiance.handle();
    to_general.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    to_general.src_access_mask = VK_ACCESS_2_NONE;
    to_general.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_general.dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_general.old_layout = graphics::image_layout::undefined;
    to_general.new_layout = graphics::image_layout::general;
    to_general.aspect_mask = irradiance.aspect();
    to_general.layer_count = 6u;
    command_buffer.transition_image_layout(to_general);

    const auto view = irradiance.create_view(graphics::image_view_type::two_dimensional_array, 0u, 1u, 0u, 6u);
    const auto output_index = bindless_table.register_storage_cube(view);
    pending_storage.push_back({output_index, view});

    bindless_table.flush_writes();

    const auto shader = shader_cache.get({"shaders/ibl/irradiance.slang", entry_points});
    auto pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{.shader = shader, .name = "IBL Irradiance"});

    struct push_data {
      std::uint32_t source_index;
      std::uint32_t sampler_index;
      std::uint32_t output_index;
    }; // struct push_data

    auto push = push_data{radiance_cube_sampled_index, sampler_index, output_index};

    auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
    std::memcpy(range.data(), &push, sizeof(push));

    command_buffer.bind_pipeline(*pipeline);
    command_buffer.push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

    const auto groups = (irradiance_cube_size + threads_per_group - 1u) / threads_per_group;
    command_buffer.dispatch(groups, groups, 6u);
  }

  const auto irradiance_index = bindless_table.register_sampled_cube(irradiance.view());

  // --- Prefiltered specular: a real mip chain now, one dispatch per mip with a roughness push
  // constant, rather than N unrelated discrete images blended in the shader. ---

  const auto prefiltered_handle = registry.emplace<graphics::image>(graphics::image::create_info{
    .extent = math::vector3u{prefiltered_cube_size, prefiltered_cube_size, 1u},
    .format = graphics::format::r16g16b16a16_sfloat,
    .usage = graphics::image_usage::storage | graphics::image_usage::sampled,
    .mip_levels = prefiltered_mip_count,
    .array_layers = 6u,
    .view_type = graphics::image_view_type::cube,
    .concurrent_sharing = true,
    .name = "IBL Prefiltered"
  });

  auto& prefiltered = registry.get<graphics::image>(prefiltered_handle);

  {
    auto to_general = graphics::command_buffer::image_transition_data{};
    to_general.image = prefiltered.handle();
    to_general.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    to_general.src_access_mask = VK_ACCESS_2_NONE;
    to_general.dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_general.dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_general.old_layout = graphics::image_layout::undefined;
    to_general.new_layout = graphics::image_layout::general;
    to_general.aspect_mask = prefiltered.aspect();
    to_general.mip_levels = prefiltered_mip_count;
    to_general.layer_count = 6u;
    command_buffer.transition_image_layout(to_general);

    const auto shader = shader_cache.get({"shaders/ibl/prefilter.slang", entry_points});
    auto pipeline = compute_pipeline_cache.get(graphics::compute_pipeline::create_info{.shader = shader, .name = "IBL Prefilter"});

    command_buffer.bind_pipeline(*pipeline);

    struct push_data {
      std::uint32_t source_index;
      std::uint32_t sampler_index;
      std::uint32_t output_index;
      std::float_t roughness;
    }; // struct push_data

    for (auto mip = std::uint32_t{0u}; mip < prefiltered_mip_count; ++mip) {
      const auto mip_size = std::max(prefiltered_cube_size >> mip, 1u);

      const auto view = prefiltered.create_view(graphics::image_view_type::two_dimensional_array, mip, 1u, 0u, 6u);
      const auto output_index = bindless_table.register_storage_cube(view);
      pending_storage.push_back({output_index, view});

      bindless_table.flush_writes();

      const auto roughness = static_cast<std::float_t>(mip) / static_cast<std::float_t>(prefiltered_mip_count - 1u);

      auto push = push_data{radiance_cube_sampled_index, sampler_index, output_index, roughness};

      auto range = std::array<std::byte, graphics::bindless_table::push_constant_size>{};
      std::memcpy(range.data(), &push, sizeof(push));

      command_buffer.push_constants(bindless_table.pipeline_layout(), graphics::bindless_table::push_constant_stages, 0u, range);

      const auto groups = (mip_size + threads_per_group - 1u) / threads_per_group;
      command_buffer.dispatch(groups, groups, 6u);
    }
  }

  const auto prefiltered_index = bindless_table.register_sampled_cube(prefiltered.view());

  // --- BRDF LUT: environment-independent, baked once and shared by everything ---

  _ensure_brdf_lut(command_buffer);

  // --- Every persistent output goes shader-readable, then one blocking submit ---

  {
    auto to_read = graphics::command_buffer::image_transition_data{};
    to_read.image = irradiance.handle();
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = graphics::image_layout::general;
    to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
    to_read.aspect_mask = irradiance.aspect();
    to_read.layer_count = 6u;
    command_buffer.transition_image_layout(to_read);
  }

  {
    auto to_read = graphics::command_buffer::image_transition_data{};
    to_read.image = prefiltered.handle();
    to_read.src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    to_read.src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT;
    to_read.dst_stage_mask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    to_read.dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT;
    to_read.old_layout = graphics::image_layout::general;
    to_read.new_layout = graphics::image_layout::shader_read_only_optimal;
    to_read.aspect_mask = prefiltered.aspect();
    to_read.mip_levels = prefiltered_mip_count;
    to_read.layer_count = 6u;
    command_buffer.transition_image_layout(to_read);
  }

  command_buffer.submit_idle();

  // Only now that the GPU has actually finished is it safe to recycle these slots (see the
  // comment on transient_storage above) and destroy their transient views.
  for (const auto& entry : pending_storage) {
    bindless_table.unregister_storage_cube(entry.index);
    vkDestroyImageView(graphics_module.logical_device(), entry.view, nullptr);
  }

  for (const auto index : pending_sampled) {
    bindless_table.unregister_sampled_cube(index);
  }

  record._radiance_index = radiance_index;
  record._irradiance_index = irradiance_index;
  record._prefiltered_index = prefiltered_index;
  record._prefiltered_mip_count = prefiltered_mip_count;

  utility::logger<"assets">::info("Baked IBL for environment {}", record.id());
}

} // namespace sbx::assets
