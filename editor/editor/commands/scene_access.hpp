// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_SCENE_ACCESS_HPP_
#define EDITOR_COMMANDS_SCENE_ACCESS_HPP_

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace editor {

/**
 * @brief The active scene, re-fetched on demand — commands never hold a scene& of their own (see
 * command.hpp's doc comment on why), they call this every time execute()/undo() runs instead.
 */
[[nodiscard]] inline auto active_scene() -> sbx::scenes::scene& {
  return sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();
}

} // namespace editor

#endif // EDITOR_COMMANDS_SCENE_ACCESS_HPP_
