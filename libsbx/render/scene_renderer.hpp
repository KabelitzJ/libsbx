// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SCENE_RENDERER_HPP_
#define LIBSBX_RENDER_SCENE_RENDERER_HPP_

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

namespace sbx::render {

/**
 * @brief The 3D half of presentation_module's renderer interfaces; at most one is registered.
 *
 * prepare() is the only safe place to touch the ECS; record() may run on the render thread and uses only what prepare() extracted.
 */
class scene_renderer {

public:

  virtual ~scene_renderer() = default;

  /** @brief Extracts the frame on the main thread, before it is kicked. */
  virtual auto prepare() -> void = 0;

  /**
   * @brief Records the frame, possibly on the render thread.
   *
   * @param command_buffer The command buffer to record into.
   * @param extent The swapchain extent.
   */
  virtual auto record(graphics::command_buffer& command_buffer, math::vector2u extent) -> void = 0;

}; // class scene_renderer

} // namespace sbx::render

#endif // LIBSBX_RENDER_SCENE_RENDERER_HPP_
