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

/** @brief Per-frame compositor input: command buffer plus swapchain view and extent. */
struct compositor_context {
  memory::observer_ptr<graphics::command_buffer> command_buffer;
  VkImageView swapchain_view;
  math::vector2u swapchain_extent;
}; // struct compositor_context

/** @brief The final step deciding what ends up in the swapchain image. At most one; without one, presentation_module clears the swapchain. */
class compositor : public utility::noncopyable {

public:

  virtual ~compositor() = default;

  virtual auto execute(compositor_context& context) -> void = 0;

}; // class compositor

/**
 * @brief Clears the swapchain view to a plain dark color; the fallback without a compositor or camera.
 *
 * @param command_buffer The command buffer to record into.
 * @param swapchain_view The swapchain image view.
 * @param extent The swapchain extent.
 */
auto clear_swapchain(graphics::command_buffer& command_buffer, VkImageView swapchain_view, math::vector2u extent) -> void;

} // namespace sbx::render

#endif // LIBSBX_RENDER_COMPOSITOR_HPP_
