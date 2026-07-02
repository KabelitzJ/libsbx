// SPDX-License-Identifier: MIT
#include <libsbx/graphics/environment_map_serializer.hpp>

#include <libsbx/assets/serializer_registry.hpp>

#include <libsbx/reflection/description.hpp>

#include <libsbx/graphics/descriptor/descriptor_handler.hpp>
#include <libsbx/graphics/pipeline/compute_pipeline.hpp>
#include <libsbx/graphics/types.hpp>

namespace sbx::graphics {

auto environment_map_serializer::type() const -> std::string_view {
  return environment_map::type_name;
}

auto environment_map_serializer::read(const assets::serializer_context& context) -> std::unique_ptr<assets::asset> {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& assets_module = core::engine::get_module<assets::assets_module>();

  const auto node = YAML::LoadFile(context.resolved.string());

  const auto cube_uri = node["cube"].as<std::string>();
  const auto suffix = node["suffix"].as<std::string>(".png");
  const auto format = reflection::from_string_or<graphics::format>(node["format"].as<std::string>(""), graphics::format::r8g8b8a8_srgb);
  const auto irradiance_size = node["irradiance_size"].as<std::uint32_t>(64u);
  const auto prefiltered_size = node["prefiltered_size"].as<std::uint32_t>(512u);
  const auto brdf_size = node["brdf_size"].as<std::uint32_t>(512u);

  const auto cube_path = assets_module.resolve_path(cube_uri);

  const auto cube = graphics_module.add_resource<graphics::cube_image>(cube_path, suffix, format);

  const auto brdf = _generate_brdf(brdf_size);
  const auto irradiance = _generate_irradiance(cube, irradiance_size);
  const auto prefiltered = _generate_prefiltered(cube, prefiltered_size);

  return std::make_unique<environment_map>(cube, brdf, irradiance, prefiltered);
}

auto environment_map_serializer::write(const assets::serializer_context& context, const std::unique_ptr<assets::asset>& asset) -> bool {
  return true;
}

auto environment_map_serializer::_generate_brdf(const std::uint32_t size) -> graphics::image2d_handle {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();
  const auto& logical_device = graphics_module.logical_device();

  const auto& graphics_queue = logical_device.queue<sbx::graphics::queue::type::graphics>();
  const auto& compute_queue = logical_device.queue<sbx::graphics::queue::type::compute>();

  const auto handle = graphics_module.add_resource<sbx::graphics::image2d>(sbx::math::vector2u{size}, sbx::graphics::format::r16g16b16a16_sfloat, sbx::graphics::filter::linear, sbx::graphics::address_mode::repeat);

  auto timer = sbx::utility::timer{};

  auto& brdf = graphics_module.get_resource<sbx::graphics::image2d>(handle);

  auto initial_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

  sbx::graphics::image::transition_image_layout(initial_command_buffer, brdf.handle(), VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, brdf.mip_levels(), 0, brdf.array_layers(), 0);

  if (graphics_queue.family() != compute_queue.family()) {
    auto brdf_release = sbx::graphics::command_buffer::image_release_data{
      .image = brdf.handle(),
      .mip_levels = brdf.mip_levels(),
      .base_array_layer = 0,
      .layer_count = brdf.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    initial_command_buffer.release_image_ownership({brdf_release});
  }

  initial_command_buffer.submit_idle();

  auto compute_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::compute, true};

  if (graphics_queue.family() != compute_queue.family()) {
    auto brdf_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = brdf.handle(),
      .mip_levels = brdf.mip_levels(),
      .base_array_layer = 0,
      .layer_count = brdf.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    compute_command_buffer.acquire_image_ownership({brdf_acquire});
  }

  auto pipeline = sbx::graphics::compute_pipeline{"engine://shaders/ibl/brdf"};

  pipeline.bind(compute_command_buffer);

  auto descriptor_handler = sbx::graphics::descriptor_handler{pipeline, 0u};

  descriptor_handler.push("output", brdf);

  descriptor_handler.update(pipeline);
  descriptor_handler.bind_descriptors(compute_command_buffer);

  const auto group_count_x = (brdf.size().x() + threads_per_group - 1) / threads_per_group;
  const auto group_count_y = (brdf.size().y() + threads_per_group - 1) / threads_per_group;

  pipeline.dispatch(compute_command_buffer, sbx::math::vector3u{group_count_x, group_count_y, 1u});

  if (graphics_queue.family() != compute_queue.family()) {
    auto brdf_release = sbx::graphics::command_buffer::image_release_data{
      .image = brdf.handle(),
      .mip_levels = brdf.mip_levels(),
      .base_array_layer = 0,
      .layer_count = brdf.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    compute_command_buffer.release_image_ownership({brdf_release});
  } else {
    sbx::graphics::image::transition_image_layout(compute_command_buffer, brdf.handle(), VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, brdf.mip_levels(), 0, brdf.array_layers(), 0);
  }

  compute_command_buffer.submit_idle();

  if (graphics_queue.family() != compute_queue.family()) {
    auto final_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

    auto brdf_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = brdf.handle(),
      .mip_levels = brdf.mip_levels(),
      .base_array_layer = 0,
      .layer_count = brdf.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    final_command_buffer.acquire_image_ownership({brdf_acquire});

    final_command_buffer.submit_idle();
  }

  sbx::utility::logger<"editor">::info("Generated 'brdf' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));

  return handle;
}

auto environment_map_serializer::_generate_irradiance(const cube_image2d_handle source, const std::uint32_t size) -> graphics::cube_image2d_handle {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
  const auto& graphics_queue = logical_device.queue<sbx::graphics::queue::type::graphics>();
  const auto& compute_queue = logical_device.queue<sbx::graphics::queue::type::compute>();

  const auto handle = graphics_module.add_resource<sbx::graphics::cube_image>(sbx::math::vector2u{size}, VK_FORMAT_R32G32B32A32_SFLOAT);

  auto timer = sbx::utility::timer{};

  auto& irradiance = graphics_module.get_resource<sbx::graphics::cube_image>(handle);
  auto& skybox = graphics_module.get_resource<sbx::graphics::cube_image>(source);

  auto initial_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

  sbx::graphics::image::transition_image_layout(initial_command_buffer, irradiance.handle(), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, irradiance.mip_levels(), 0, irradiance.array_layers(), 0);

  if (graphics_queue.family() != compute_queue.family()) {
    auto irradiance_release = sbx::graphics::command_buffer::image_release_data{
      .image = irradiance.handle(),
      .mip_levels = irradiance.mip_levels(),
      .base_array_layer = 0,
      .layer_count = irradiance.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    auto skybox_release = sbx::graphics::command_buffer::image_release_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    initial_command_buffer.release_image_ownership({irradiance_release, skybox_release});
  }

  initial_command_buffer.submit_idle();

  auto compute_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::compute, true};

  if (graphics_queue.family() != compute_queue.family()) {
    auto irradiance_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = irradiance.handle(),
      .mip_levels = irradiance.mip_levels(),
      .base_array_layer = 0,
      .layer_count = irradiance.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    auto skybox_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    compute_command_buffer.acquire_image_ownership({irradiance_acquire, skybox_acquire});
  }

  auto pipeline = sbx::graphics::compute_pipeline{"engine://shaders/ibl/irradiance"};

  pipeline.bind(compute_command_buffer);

  auto descriptor_handler = sbx::graphics::descriptor_handler{pipeline, 0u};

  descriptor_handler.push("skybox", skybox);
  descriptor_handler.push("output", irradiance);

  descriptor_handler.update(pipeline);
  descriptor_handler.bind_descriptors(compute_command_buffer);

  const auto group_count_x = (irradiance.size().x() + threads_per_group - 1) / threads_per_group;
  const auto group_count_y = (irradiance.size().y() + threads_per_group - 1) / threads_per_group;

  pipeline.dispatch(compute_command_buffer, sbx::math::vector3u{group_count_x, group_count_y, 6u});

  if (graphics_queue.family() != compute_queue.family()) {
    auto irradiance_release = sbx::graphics::command_buffer::image_release_data{
      .image = irradiance.handle(),
      .mip_levels = irradiance.mip_levels(),
      .base_array_layer = 0,
      .layer_count = irradiance.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    auto skybox_release = sbx::graphics::command_buffer::image_release_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    compute_command_buffer.release_image_ownership({irradiance_release, skybox_release});
  } else {
    sbx::graphics::image::transition_image_layout(compute_command_buffer, irradiance.handle(), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, irradiance.mip_levels(), 0, irradiance.array_layers(), 0);
  }

  compute_command_buffer.submit_idle();

  if (graphics_queue.family() != compute_queue.family()) {
    auto final_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

    auto irradiance_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = irradiance.handle(),
      .mip_levels = irradiance.mip_levels(),
      .base_array_layer = 0,
      .layer_count = irradiance.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    auto skybox_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    final_command_buffer.acquire_image_ownership({irradiance_acquire, skybox_acquire});

    final_command_buffer.submit_idle();
  }

  sbx::utility::logger<"editor">::info("Generated 'irradiance' in {:.2f}ms", sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));

  return handle;
}

auto environment_map_serializer::_generate_prefiltered(const cube_image2d_handle source, const std::uint32_t size) -> graphics::cube_image2d_handle {
  constexpr auto threads_per_group = std::uint32_t{16};

  auto& graphics_module = sbx::core::engine::get_module<sbx::graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
  const auto& graphics_queue = logical_device.queue<sbx::graphics::queue::type::graphics>();
  const auto& compute_queue = logical_device.queue<sbx::graphics::queue::type::compute>();

  const auto handle = graphics_module.add_resource<sbx::graphics::cube_image>(sbx::math::vector2u{size}, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLE_COUNT_1_BIT, true, true);

  auto timer = sbx::utility::timer{};

  auto& prefiltered = graphics_module.get_resource<sbx::graphics::cube_image>(handle);
  auto& skybox = graphics_module.get_resource<sbx::graphics::cube_image>(source);

  auto initial_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

  sbx::graphics::image::transition_image_layout(initial_command_buffer, prefiltered.handle(), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, prefiltered.mip_levels(), 0, prefiltered.array_layers(), 0);

  if (graphics_queue.family() != compute_queue.family()) {
    auto prefiltered_release = sbx::graphics::command_buffer::image_release_data{
      .image = prefiltered.handle(),
      .mip_levels = prefiltered.mip_levels(),
      .base_array_layer = 0,
      .layer_count = prefiltered.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .src_access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    auto skybox_release = sbx::graphics::command_buffer::image_release_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    initial_command_buffer.release_image_ownership({prefiltered_release, skybox_release});
  }

  initial_command_buffer.submit_idle();

  auto compute_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::compute, true};

  if (graphics_queue.family() != compute_queue.family()) {
    auto prefiltered_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = prefiltered.handle(),
      .mip_levels = prefiltered.mip_levels(),
      .base_array_layer = 0,
      .layer_count = prefiltered.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_GENERAL
    };

    auto skybox_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = graphics_queue.family(),
      .dst_queue_family = compute_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    compute_command_buffer.acquire_image_ownership({prefiltered_acquire, skybox_acquire});
  }

  auto pipeline = sbx::graphics::compute_pipeline{"engine://shaders/ibl/prefiltered"};
  auto push_handler = sbx::graphics::push_handler{pipeline};

  pipeline.bind(compute_command_buffer);

  auto descriptor_handlers = std::vector<sbx::graphics::descriptor_handler>{};
  descriptor_handlers.reserve(prefiltered.mip_levels());

  for (auto mip = 0u; mip < prefiltered.mip_levels(); ++mip) {
    descriptor_handlers.emplace_back(pipeline, 0u);
  }

  auto mip_views = std::vector<VkImageView>{};
  mip_views.resize(prefiltered.mip_levels());

  for (auto mip = 0u; mip < prefiltered.mip_levels(); ++mip) {
    sbx::graphics::image::create_image_view(prefiltered, mip_views[mip], VK_IMAGE_VIEW_TYPE_2D_ARRAY, prefiltered.format(), VK_IMAGE_ASPECT_COLOR_BIT, 1, mip, 6, 0);
  }

  for (auto mip = 0u; mip < prefiltered.mip_levels(); ++mip) {
    auto barrier = VkImageMemoryBarrier2{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = prefiltered;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = mip;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;

    auto dependency = VkDependencyInfo{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(compute_command_buffer, &dependency);

    auto& descriptor_handler = descriptor_handlers[mip];

    auto descriptor_image_info = std::vector<VkDescriptorImageInfo>{};
    descriptor_image_info.reserve(1u);

    descriptor_image_info.push_back(VkDescriptorImageInfo{
      .sampler = VK_NULL_HANDLE,
      .imageView = mip_views[mip],
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    });

    auto descriptor_write = VkWriteDescriptorSet{};
    descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_write.dstBinding = *pipeline.find_descriptor_binding("output", 0u);
    descriptor_write.dstArrayElement = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = *pipeline.find_descriptor_type_at_binding(0u, descriptor_write.dstBinding);

    auto write_set = sbx::graphics::write_descriptor_set{descriptor_write, descriptor_image_info};

    descriptor_handler.push("skybox", skybox);
    descriptor_handler.push("output", prefiltered, std::move(write_set));

    descriptor_handler.update(pipeline);
    descriptor_handler.bind_descriptors(compute_command_buffer);

    const auto roughness = static_cast<std::float_t>(mip) / static_cast<std::float_t>(prefiltered.mip_levels() - 1);

    push_handler.push("roughness", roughness);
    push_handler.bind(compute_command_buffer);

    const auto group_count_x = ((prefiltered.size().x() >> mip) + threads_per_group - 1) / threads_per_group;
    const auto group_count_y = ((prefiltered.size().y() >> mip) + threads_per_group - 1) / threads_per_group;

    pipeline.dispatch(compute_command_buffer, {group_count_x, group_count_y, 1});
  }

  if (graphics_queue.family() != compute_queue.family()) {
    auto prefiltered_release = sbx::graphics::command_buffer::image_release_data{
      .image = prefiltered.handle(),
      .mip_levels = prefiltered.mip_levels(),
      .base_array_layer = 0,
      .layer_count = prefiltered.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_WRITE_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    auto skybox_release = sbx::graphics::command_buffer::image_release_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .src_stage_mask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .src_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    compute_command_buffer.release_image_ownership({prefiltered_release, skybox_release});
  } else {
    sbx::graphics::image::transition_image_layout(compute_command_buffer, prefiltered.handle(), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, prefiltered.mip_levels(), 0, prefiltered.array_layers(), 0);
  }

  compute_command_buffer.submit_idle();

  if (graphics_queue.family() != compute_queue.family()) {
    auto final_command_buffer = sbx::graphics::command_buffer{sbx::graphics::queue::type::graphics, true};

    auto prefiltered_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = prefiltered.handle(),
      .mip_levels = prefiltered.mip_levels(),
      .base_array_layer = 0,
      .layer_count = prefiltered.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_GENERAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    auto skybox_acquire = sbx::graphics::command_buffer::image_acquire_data{
      .image = skybox.handle(),
      .mip_levels = skybox.mip_levels(),
      .base_array_layer = 0,
      .layer_count = skybox.array_layers(),
      .dst_stage_mask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dst_access_mask = VK_ACCESS_2_SHADER_READ_BIT,
      .src_queue_family = compute_queue.family(),
      .dst_queue_family = graphics_queue.family(),
      .old_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    final_command_buffer.acquire_image_ownership({prefiltered_acquire, skybox_acquire});

    final_command_buffer.submit_idle();
  }

  for (auto& view : mip_views) {
    vkDestroyImageView(graphics_module.logical_device(), view, nullptr);
  }

  sbx::utility::logger<"editor">::info("Generated 'prefiltered' with {} mips in {:.2f}ms", prefiltered.mip_levels(), sbx::units::quantity_cast<sbx::units::millisecond>(timer.elapsed()));

  return handle;
}

} // namespace sbx::graphics
