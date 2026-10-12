// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <runtime/application.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/input.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/components.hpp>


#include <libsbx/render/scene_renderer_module.hpp>

namespace runtime {

application::application()
: sbx::core::application{}, 
  _is_paused{false} {
  auto& platform_module = sbx::core::engine::get_module<sbx::platform::platform_module>();

  auto& window = platform_module.window();

  window.on_window_closed() += []([[maybe_unused]] const auto& event) {
    sbx::core::engine::quit();
  };

  auto& project = sbx::core::engine::project();

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  assets_module.import_directory(project.assets_directory());

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();

  // Like a Unity build: only scenes in the project's scene list can be loaded.
  scenes_module.set_require_scene_list(true);

  // Through the same path as SceneManager.LoadScene, whose scene-loaded signal starts the scene's scripts.
  if (const auto startup_scene = project.startup_scene()) {
    scenes_module.load_scene(*startup_scene);
    scenes_module.apply_pending_operations();
  } else {
    sbx::utility::logger<"runtime">::warn("The project's scene list has no enabled scene to start with (Project Settings > Scenes)");
  }
}

application::~application() {

}

auto application::update() -> void {

}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

} // namespace runtime
