// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_COMMANDS_MIP_CHAIN_HPP_
#define LIBSBX_GRAPHICS_COMMANDS_MIP_CHAIN_HPP_

#include <vulkan/vulkan.h>

#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/image.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

namespace sbx::graphics {

/**
 * @brief The pipeline stage/access scope a resource transitioning *into* @p layout should
 * synchronise against. Shared by generate_mip_chain and upload_context.
 */
struct access_scope {
  VkPipelineStageFlags2 stage;
  VkAccessFlags2 access;
}; // struct access_scope

[[nodiscard]] auto scope_for_layout(image_layout layout) -> access_scope;

/**
 * @brief The pipeline stage/access mask/layout mip 0 of the target image sits in right before
 * generate_mip_chain runs — i.e. whatever last wrote it (a buffer->image copy, or a compute
 * shader write).
 */
struct mip_chain_source {
  image_layout layout;
  VkPipelineStageFlags2 stage_mask;
  VkAccessFlags2 access_mask;
}; // struct mip_chain_source

/**
 * @brief Fills mips [1, target.mip_levels()) of @p target from its mip 0, one mip at a time via
 * linear vkCmdBlitImage, across every array layer. Mip 0 must already hold valid data in the
 * scope described by @p source. Leaves every mip of @p target in @p final_layout.
 *
 * A no-op if @p target has a single mip level.
 *
 * @p target must have been created with both `image_usage::transfer_source` and
 * `image_usage::transfer_destination`.
 */
auto generate_mip_chain(command_buffer& commands, const image& target, const mip_chain_source& source, image_layout final_layout) -> void;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_COMMANDS_MIP_CHAIN_HPP_
