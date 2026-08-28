// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/commands/mip_chain.hpp>

#include <algorithm>
#include <cstdint>

namespace sbx::graphics {

auto scope_for_layout(const image_layout layout) -> access_scope {
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

auto generate_mip_chain(command_buffer& commands, const image& target, const mip_chain_source& source, image_layout final_layout) -> void {
  const auto mip_levels = target.mip_levels();

  if (mip_levels <= 1u) {
    return;
  }

  const auto layer_count = target.array_layers();
  const auto aspect = target.aspect();
  const auto handle = target.handle();

  // Mip 0 already holds valid data; make it a valid blit source.
  auto base_to_source = command_buffer::image_transition_data{};
  base_to_source.image = handle;
  base_to_source.src_stage_mask = source.stage_mask;
  base_to_source.src_access_mask = source.access_mask;
  base_to_source.dst_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  base_to_source.dst_access_mask = VK_ACCESS_2_TRANSFER_READ_BIT;
  base_to_source.old_layout = source.layout;
  base_to_source.new_layout = image_layout::transfer_source_optimal;
  base_to_source.aspect_mask = aspect;
  base_to_source.layer_count = layer_count;
  commands.transition_image_layout(base_to_source);

  auto width = static_cast<std::int32_t>(target.extent().x());
  auto height = static_cast<std::int32_t>(target.extent().y());

  for (auto mip = std::uint32_t{1u}; mip < mip_levels; ++mip) {
    const auto source_width = width;
    const auto source_height = height;

    width = std::max(width / 2, 1);
    height = std::max(height / 2, 1);

    auto to_destination = command_buffer::image_transition_data{};
    to_destination.image = handle;
    to_destination.src_stage_mask = VK_PIPELINE_STAGE_2_NONE;
    to_destination.src_access_mask = VK_ACCESS_2_NONE;
    to_destination.dst_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_destination.dst_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_destination.old_layout = image_layout::undefined;
    to_destination.new_layout = image_layout::transfer_destination_optimal;
    to_destination.aspect_mask = aspect;
    to_destination.base_mip_level = mip;
    to_destination.layer_count = layer_count;
    commands.transition_image_layout(to_destination);

    auto blit = VkImageBlit{};
    blit.srcSubresource = VkImageSubresourceLayers{aspect, mip - 1u, 0u, layer_count};
    blit.srcOffsets[1] = VkOffset3D{source_width, source_height, 1};
    blit.dstSubresource = VkImageSubresourceLayers{aspect, mip, 0u, layer_count};
    blit.dstOffsets[1] = VkOffset3D{width, height, 1};

    vkCmdBlitImage(commands.handle(), handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &blit, VK_FILTER_LINEAR);

    auto to_source = command_buffer::image_transition_data{};
    to_source.image = handle;
    to_source.src_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_source.src_access_mask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_source.dst_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_source.dst_access_mask = VK_ACCESS_2_TRANSFER_READ_BIT;
    to_source.old_layout = image_layout::transfer_destination_optimal;
    to_source.new_layout = image_layout::transfer_source_optimal;
    to_source.aspect_mask = aspect;
    to_source.base_mip_level = mip;
    to_source.layer_count = layer_count;
    commands.transition_image_layout(to_source);
  }

  // Every mip now sits in TRANSFER_SRC_OPTIMAL; move the whole chain to its final layout at once.
  const auto final_scope = scope_for_layout(final_layout);

  auto to_final = command_buffer::image_transition_data{};
  to_final.image = handle;
  to_final.src_stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  to_final.src_access_mask = VK_ACCESS_2_TRANSFER_READ_BIT;
  to_final.dst_stage_mask = final_scope.stage;
  to_final.dst_access_mask = final_scope.access;
  to_final.old_layout = image_layout::transfer_source_optimal;
  to_final.new_layout = final_layout;
  to_final.aspect_mask = aspect;
  to_final.mip_levels = mip_levels;
  to_final.layer_count = layer_count;
  commands.transition_image_layout(to_final);
}

} // namespace sbx::graphics
