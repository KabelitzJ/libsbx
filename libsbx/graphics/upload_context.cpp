// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/upload_context.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/commands/mip_chain.hpp>

namespace sbx::graphics {

auto upload_context::stage_image(image_handle destination, std::span<const std::byte> pixels, image_layout final_layout) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();

  const auto& destination_image = registry.get<image>(destination);

  const auto staging = registry.emplace<buffer>(buffer::create_info{
    .size = pixels.size(),
    .usage = buffer_usage::transfer_source,
    .memory = memory_usage::host_write,
    .name = "Staging"
  });

  registry.get<buffer>(staging).write(pixels.data(), pixels.size());

  _pending_images.push_back(pending_image{
    .destination = destination,
    .staging = staging,
    .extent = destination_image.extent(),
    .mip_levels = destination_image.mip_levels(),
    .array_layers = destination_image.array_layers(),
    .aspect = destination_image.aspect(),
    .final_layout = final_layout
  });
}

auto upload_context::stage_buffer(buffer_handle destination, std::span<const std::byte> data, buffer::size_type destination_offset) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();

  const auto staging = registry.emplace<buffer>(buffer::create_info{
    .size = data.size(),
    .usage = buffer_usage::transfer_source,
    .memory = memory_usage::host_write,
    .name = "Staging"
  });

  registry.get<buffer>(staging).write(data.data(), data.size());

  _pending_buffers.push_back(pending_buffer{
    .destination = destination,
    .staging = staging,
    .size = data.size(),
    .destination_offset = destination_offset
  });
}

auto upload_context::flush(command_buffer& commands, std::uint64_t frame_index) -> void {
  if (_pending_images.empty() && _pending_buffers.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();

  // Buffer copies first, then one global barrier making all of them visible to any later read
  if (!_pending_buffers.empty()) {
    for (const auto& pending : _pending_buffers) {
      auto& destination = registry.get<buffer>(pending.destination);
      auto& staging = registry.get<buffer>(pending.staging);

      auto region = VkBufferCopy{};
      region.srcOffset = 0u;
      region.dstOffset = pending.destination_offset;
      region.size = pending.size;

      vkCmdCopyBuffer(commands.handle(), staging.handle(), destination.handle(), 1u, &region);

      registry.retire(pending.staging, frame_index);
    }

    auto memory_barrier = VkMemoryBarrier2{};
    memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memory_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    memory_barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    memory_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    memory_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;

    auto dependency = VkDependencyInfo{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.memoryBarrierCount = 1u;
    dependency.pMemoryBarriers = &memory_barrier;

    vkCmdPipelineBarrier2(commands.handle(), &dependency);
  }

  for (const auto& pending : _pending_images) {
    auto& destination_image = registry.get<image>(pending.destination);
    auto& staging = registry.get<buffer>(pending.staging);

    auto to_transfer = command_buffer::image_transition_data{};
    to_transfer.image = destination_image.handle();
    to_transfer.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    to_transfer.src_access_mask = VK_ACCESS_2_NONE;
    to_transfer.dst_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_transfer.dst_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_transfer.old_layout = image_layout::undefined;
    to_transfer.new_layout = image_layout::transfer_destination_optimal;
    to_transfer.aspect_mask = pending.aspect;
    to_transfer.layer_count = pending.array_layers;

    commands.transition_image_layout(to_transfer);

    auto region = VkBufferImageCopy{};
    region.bufferOffset = 0u;
    region.bufferRowLength = 0u;
    region.bufferImageHeight = 0u;
    region.imageSubresource.aspectMask = pending.aspect;
    region.imageSubresource.mipLevel = 0u;
    region.imageSubresource.baseArrayLayer = 0u;
    region.imageSubresource.layerCount = pending.array_layers;
    region.imageOffset = VkOffset3D{0, 0, 0};
    region.imageExtent = VkExtent3D{pending.extent.x(), pending.extent.y(), pending.extent.z()};

    vkCmdCopyBufferToImage(commands.handle(), staging.handle(), destination_image.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);

    if (pending.mip_levels > 1u) {
      const auto source = mip_chain_source{
        .layout = image_layout::transfer_destination_optimal,
        .stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT
      };

      generate_mip_chain(commands, destination_image, source, pending.final_layout);
    } else {
      const auto final_scope = scope_for_layout(pending.final_layout);

      auto to_final = command_buffer::image_transition_data{};
      to_final.image = destination_image.handle();
      to_final.src_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      to_final.src_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      to_final.dst_stage_mask = final_scope.stage;
      to_final.dst_access_mask = final_scope.access;
      to_final.old_layout = image_layout::transfer_destination_optimal;
      to_final.new_layout = pending.final_layout;
      to_final.aspect_mask = pending.aspect;
      to_final.layer_count = pending.array_layers;

      commands.transition_image_layout(to_final);
    }

    registry.retire(pending.staging, frame_index);
  }

  _pending_images.clear();
  _pending_buffers.clear();
}

} // namespace sbx::graphics
