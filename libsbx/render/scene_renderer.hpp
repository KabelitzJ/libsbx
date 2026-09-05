// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SCENE_RENDERER_HPP_
#define LIBSBX_RENDER_SCENE_RENDERER_HPP_

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

namespace sbx::render {

/**
 * @brief 3D-scene half of presentation_module's renderer interfaces; at most one registered at a
 * time (presentation_module::set_scene_renderer).
 *
 * Two phases: prepare() is the only place touching the ECS is safe (main thread, before the frame
 * is kicked); record() does the actual pass-list work and may run on a separate render thread, so
 * it must only use what prepare() already extracted and stashed.
 */
class scene_renderer {

public:

  virtual ~scene_renderer() = default;

  /** @brief Main thread, once per frame, before the frame is kicked off. */
  virtual auto prepare() -> void = 0;

  /** @brief Render thread (or same thread, depending on threading_policy). Records into @p command_buffer. */
  virtual auto record(graphics::command_buffer& command_buffer, math::vector2u extent) -> void = 0;

}; // class scene_renderer

} // namespace sbx::render

#endif // LIBSBX_RENDER_SCENE_RENDERER_HPP_
