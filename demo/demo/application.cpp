// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <demo/application.hpp>

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

#include <libsbx/render/render_module.hpp>

namespace demo {

application::application()
: sbx::core::application{}, _is_paused{false}, _time{0}, _fps{0} {
  auto& platform_module = sbx::core::engine::get_module<sbx::platform::platform_module>();
  platform_module.window().on_window_closed() += []([[maybe_unused]] const auto& event) {
    sbx::core::engine::quit();
  };

  auto& project = sbx::core::engine::set_project(sbx::core::project::open_or_create("demo", "Demo"));

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();
  assets_module.import_directory(project.assets_directory());

  const auto duck_mesh = assets_module.load_mesh("models/duck/duck.gltf");
  const auto damaged_helmet_mesh = assets_module.load_mesh("models/damaged_helmet/damaged_helmet.gltf");
  const auto flight_helmet_mesh = assets_module.load_mesh("models/flight_helmet/flight_helmet.gltf");

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  sbx::scenes::scene_serializer::load(scene, "scenes/demo.yaml");

  _camera = scene.find("Camera");
  _duck = scene.find("Duck");
  _damaged_helmet = scene.find("DamagedHelmet");
  _flight_helmet = scene.find("FlightHelmet");

  _camera_controller = fly_camera{_camera};

  auto& skybox = _camera.add_component<sbx::scenes::skybox>();
  skybox.environment = assets_module.load_environment_map("environments/sky.hdr");
  skybox.intensity = 0.1f;
}

application::~application() {

}

auto application::update() -> void {
  using namespace sbx::units::literals;

  if (sbx::platform::input::is_key_pressed(sbx::platform::key::escape)) {
    sbx::core::engine::quit();
  }

  _rotation += sbx::math::degree{90.0f} * sbx::core::engine::delta_time();

  auto& duck_transform = _duck.transform();
  duck_transform.rotation = sbx::math::quaternion{sbx::math::vector3f{0.0f, 1.0f, 0.0f}, _rotation};

  auto& damaged_helmet_transform = _damaged_helmet.transform();
  damaged_helmet_transform.rotation = sbx::math::quaternion{sbx::math::vector3f{0.0f, 1.0f, 0.0f}, _rotation};

  // auto& flight_helmet_transform = _flight_helmet.transform();
  // flight_helmet_transform.rotation = sbx::math::quaternion{sbx::math::vector3f{0.0f, 1.0f, 0.0f}, _rotation};

  _camera_controller.update();
}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

} // namespace demo
