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
 * @brief Everything a compositor needs — deliberately leaner than the 3D pass list's
 * render_context, which carries depth/color/shadow/cluster buffers and camera data that have no
 * business leaking into presentation_module (which knows nothing about 3D rendering at all).
 */
struct compositor_context {
  memory::observer_ptr<graphics::command_buffer> command_buffer;
  VkImageView swapchain_view;
  math::vector2u swapchain_extent;
}; // struct compositor_context

/**
 * @brief The one final step deciding what ends up in the swapchain image, registered with
 * presentation_module::set_compositor (at most one). Successor to render_module's old
 * set_composite_pass/render_pass-based composite pass — scene_renderer_module registers
 * scene_blit_compositor as its default; presentation_module clears the swapchain instead if
 * nothing is registered at all.
 */
class compositor : public utility::noncopyable {

public:

  virtual ~compositor() = default;

  virtual auto execute(compositor_context& context) -> void = 0;

}; // class compositor

/**
 * @brief Clears @p swapchain_view to a plain dark color and nothing else. Used both as
 * presentation_module's own fallback when no compositor is registered at all, and by
 * scene_blit_compositor when there's nothing fresh to show yet (no active camera this frame) —
 * the same two call sites render_pass.hpp's old clear_swapchain served, just no longer tied to
 * the 3D pass list's render_context.
 */
auto clear_swapchain(graphics::command_buffer& command_buffer, VkImageView swapchain_view, math::vector2u extent) -> void;

} // namespace sbx::render

#endif // LIBSBX_RENDER_COMPOSITOR_HPP_
