// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_RENDER_SCENE_RENDERER_HPP_
#define LIBSBX_RENDER_SCENE_RENDERER_HPP_

#include <libsbx/math/vector2.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

namespace sbx::render {

/**
 * @brief What presentation_module calls into, at most one registered at a time
 * (presentation_module::set_scene_renderer), to have a 3D scene rendered into the frame's shared
 * command buffer. scene_renderer_module is the only real implementer — named to pair with it the
 * same way ui_layer already pairs with editor_ui_layer.
 *
 * Two phases, mirroring ui_renderer's shape, for the same reason: prepare() is the only place
 * touching the ECS is safe (main thread, called once per frame before the frame is kicked off);
 * record() does the actual pass-list work, and may run on a separate render thread depending on
 * the configured threading_policy — it must never reach into the ECS itself, only whatever
 * prepare() already extracted and stashed.
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
