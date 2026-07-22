// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/upload_context.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

namespace sbx::graphics {

struct access_scope {
  VkPipelineStageFlags2 stage;
  VkAccessFlags2 access;
}; // struct access_scope

auto _scope_for_layout(const image_layout layout) -> access_scope {
  switch (layout) {
    case image_layout::shader_read_only_optimal: {
      return access_scope{VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_READ_BIT};
    }
    case image_layout::transfer_source_optimal: {
      return access_scope{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
    }
    case image_layout::transfer_destination_optimal: {
      return access_scope{VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
    }
    default: {
      return access_scope{VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT};
    }
  }
}

auto upload_context::stage_image(image_handle destination, std::span<const std::byte> pixels, image_layout final_layout) -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();

  const auto& destination_image = registry.get<image>(destination);

  const auto staging = registry.emplace<buffer>(buffer::create_info{
    .size = static_cast<buffer::size_type>(pixels.size()),
    .usage = buffer_usage::transfer_source,
    .memory = memory_usage::host_write,
    .name = "Staging"
  });

  registry.get<buffer>(staging).write(pixels.data(), static_cast<buffer::size_type>(pixels.size()));

  _pending_images.push_back(pending_image{
    .destination = destination,
    .staging = staging,
    .extent = destination_image.extent(),
    .array_layers = destination_image.array_layers(),
    .aspect = destination_image.aspect(),
    .final_layout = final_layout
  });
}

auto upload_context::flush(command_buffer& commands, std::uint64_t frame_index) -> void {
  if (_pending_images.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& registry = graphics_module.resource_registry();

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

    const auto final_scope = _scope_for_layout(pending.final_layout);

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

    registry.retire(pending.staging, frame_index);
  }

  _pending_images.clear();
}

} // namespace sbx::graphics
