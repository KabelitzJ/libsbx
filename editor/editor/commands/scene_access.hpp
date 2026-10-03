// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_SCENE_ACCESS_HPP_
#define EDITOR_COMMANDS_SCENE_ACCESS_HPP_

#include <libsbx/core/engine.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scenes_module.hpp>

namespace editor {

/**
 * @brief The active scene, fetched fresh each time, since commands never hold a scene reference.
 *
 * @return The active scene.
 */
[[nodiscard]] inline auto active_scene() -> sbx::scenes::scene& {
  return sbx::core::engine::get_module<sbx::scenes::scenes_module>().active_scene();
}

} // namespace editor

#endif // EDITOR_COMMANDS_SCENE_ACCESS_HPP_
