// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <editor/application.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/core/engine.hpp>
#include <libsbx/core/project.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/input.hpp>

#include <libsbx/filesystem/filesystem_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>
#include <libsbx/scenes/scene_serializer.hpp>
#include <libsbx/scenes/components.hpp>

#include <libsbx/scripting/scripting_module.hpp>

#include <libsbx/render/scene_renderer_module.hpp>

#include <editor/editor_module.hpp>

namespace editor {

application::application()
: sbx::core::application{},
  _is_paused{false}, 
  _time{0}, 
  _fps{0} {
  auto& platform_module = sbx::core::engine::get_module<sbx::platform::platform_module>();

  auto& window = platform_module.window();

  window.on_window_closed() += []([[maybe_unused]] const auto& event) {
    sbx::core::engine::get_module<editor::editor_module>().request_quit();
  };

  auto& project = sbx::core::engine::project();

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  assets_module.import_directory(project.assets_directory());

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  // A fresh/projectless-launcher-created project has no startup_scene — start from whatever
  // empty scene scenes_module already handed us instead of assuming one exists on disk.
  if (const auto& startup_scene = project.startup_scene()) {
    sbx::scenes::scene_serializer::load(scene, *startup_scene);
    editor_module.set_scene_path(*startup_scene);
  }

  _camera = scene.active_camera();

  // scene_serializer::load only sets an active camera if the loaded scene had one, so a fresh
  // project can't assume a "Camera" node exists — create a default one.
  if (!_camera.is_valid()) {
    _camera = scene.create_node("Camera");
    _camera.add_component<sbx::scenes::camera>();
    scene.set_active_camera(_camera);
  }

  _camera_controller = fly_camera{_camera};

  if (!_camera.has_component<sbx::scenes::skybox>()) {
    auto& skybox = _camera.add_component<sbx::scenes::skybox>();
    skybox.environment = assets_module.load_environment_map("environments/sky.hdr");
    skybox.intensity = 1.0f;
  }

  auto& scripting_module = sbx::core::engine::get_module<sbx::scripting::scripting_module>();

  // Relative to the executable's own directory (see EDITOR_DOTNET_OUT_DIR in editor/CMakeLists.txt),
  // not the process's CWD — a hardcoded path only worked when launched from the repo root.
  const auto assembly_path = sbx::filesystem::executable_directory() / ".." / "dotnet" / "editor" / "Editor.dll";

  scripting_module.load_assembly(assembly_path);

  scripting_module.instantiate(_camera, "Editor.Test");
}

application::~application() {

}

auto application::update() -> void {
  using namespace sbx::units::literals;

  _rotation += sbx::math::degree{90.0f} * sbx::core::engine::delta_time();

  auto& editor_module = sbx::core::engine::get_module<editor::editor_module>();

  if (sbx::platform::input::is_mouse_button_pressed(sbx::platform::mouse_button::right) && editor_module.is_viewport_hovered()) {
    _camera_is_engaged = true;
  }

  if (!sbx::platform::input::is_mouse_button_down(sbx::platform::mouse_button::right)) {
    _camera_is_engaged = false;
  }

  if (_camera_is_engaged) {
    _camera_controller.update();
  }
}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

} // namespace editor
