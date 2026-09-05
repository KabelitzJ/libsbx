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
#include <libsbx/scenes/scene_serializer.hpp>
#include <libsbx/scenes/components.hpp>

#include <libsbx/scripting/scripting_module.hpp>

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
  auto& scene = scenes_module.active_scene();

  if (const auto& startup_scene = project.startup_scene()) {
    sbx::scenes::scene_serializer::load(scene, *startup_scene);

    auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();
    scripting_module.instantiate_scene_scripts(scene);
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
