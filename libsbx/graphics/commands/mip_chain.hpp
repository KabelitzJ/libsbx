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
 * @brief The pipeline stage/access scope a resource transitioning into a layout must synchronize against.
 *
 * Shared by @ref generate_mip_chain and @ref upload_context.
 */
struct access_scope {
  VkPipelineStageFlags2 stage;
  VkAccessFlags2 access;
}; // struct access_scope

[[nodiscard]] auto scope_for_layout(image_layout layout) -> access_scope;

/**
 * @brief The pipeline stage/access mask/layout that mip 0 of the target image holds immediately before @ref generate_mip_chain runs.
 */
struct mip_chain_source {
  image_layout layout;
  VkPipelineStageFlags2 stage_mask;
  VkAccessFlags2 access_mask;
}; // struct mip_chain_source

/**
 * @brief Fills mips [1, target.mip_levels()) of @p target from mip 0 via successive linear blits, across every array layer.
 *
 * Mip 0 must already hold valid data in the scope described by @p source; every mip ends in @p final_layout. A no-op if @p target has a single mip level.
 *
 * @p target must have been created with both `image_usage::transfer_source` and `image_usage::transfer_destination`.
 */
auto generate_mip_chain(command_buffer& commands, const image& target, const mip_chain_source& source, image_layout final_layout) -> void;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_COMMANDS_MIP_CHAIN_HPP_
