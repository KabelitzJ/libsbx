// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_COMPOSITOR_HPP_
#define LIBSBX_RENDER_COMPOSITOR_HPP_

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

namespace sbx::render {

/**
 * @brief Minimal per-frame context a compositor needs: command buffer plus swapchain view/extent.
 */
struct compositor_context {
  memory::observer_ptr<graphics::command_buffer> command_buffer;
  VkImageView swapchain_view;
  math::vector2u swapchain_extent;
}; // struct compositor_context

/**
 * @brief The final step deciding what ends up in the swapchain image.
 *
 * Registered via presentation_module::set_compositor (at most one); presentation_module clears
 * the swapchain instead if nothing is registered.
 */
class compositor : public utility::noncopyable {

public:

  virtual ~compositor() = default;

  virtual auto execute(compositor_context& context) -> void = 0;

}; // class compositor

/**
 * @brief Clears @p swapchain_view to a plain dark color.
 *
 * Used as presentation_module's fallback when no compositor is registered, and by
 * scene_blit_compositor when nothing was rendered this frame (no active camera).
 */
auto clear_swapchain(graphics::command_buffer& command_buffer, VkImageView swapchain_view, math::vector2u extent) -> void;

} // namespace sbx::render

#endif // LIBSBX_RENDER_COMPOSITOR_HPP_
