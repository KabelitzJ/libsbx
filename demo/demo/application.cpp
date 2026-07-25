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

#include <libsbx/render/render_module.hpp>

namespace demo {

application::application()
: sbx::core::application{},
  _is_paused{false},
  _time{0},
  _fps{0} {
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

  assets_module.save_material(damaged_helmet_mesh->submeshes().front().material, "materials/damaged_helmet.material");

  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  _duck = scene.create_node("Duck");
  auto& duck_transform = _duck.transform();
  duck_transform.position = sbx::math::vector3f{0.0f, 0.0f, 0.0f};

  auto& duck_renderer = _duck.add_component<sbx::scenes::mesh_renderer>();
  duck_renderer.mesh = duck_mesh;

  _damaged_helmet = scene.create_node("Damaged Helmet");
  auto& damaged_helmet_transform = _damaged_helmet.transform();
  damaged_helmet_transform.position = sbx::math::vector3f{-2.0f, 0.0f, 0.0f};

  auto& damaged_helmet_renderer = _damaged_helmet.add_component<sbx::scenes::mesh_renderer>();
  damaged_helmet_renderer.mesh = damaged_helmet_mesh;

  _flight_helmet = scene.create_node("Flight Helmet");
  auto& flight_helmet_transform = _flight_helmet.transform();
  flight_helmet_transform.position = sbx::math::vector3f{2.0f, 0.0f, 0.0f};

  auto& flight_helmet_renderer = _flight_helmet.add_component<sbx::scenes::mesh_renderer>();
  flight_helmet_renderer.mesh = flight_helmet_mesh;

  auto light = scene.create_node("Light");
  auto& light_transform = light.transform();
  light_transform.position = sbx::math::vector3f{0.0f, 3.0f, 0.0f};

  auto& light_component = light.add_component<sbx::scenes::point_light>();
  light_component.color = sbx::math::color{1.0f, 0.0f, 0.0f, 1.0f};
  light_component.intensity = 10.0f;
  light_component.range = 20.0f;

  auto camera = scene.create_node("Camera");
  camera.transform().position = sbx::math::vector3f{0.0f, 0.0f, 4.0f};

  auto& camera_component = camera.add_component<sbx::scenes::camera>();
  camera_component.fov_degrees = 60.0f;
  camera_component.near_plane = 0.01f;
  camera_component.far_plane = 1000.0f;

  scene.set_active_camera(camera);

  auto sun = scene.create_node("Sun");

  auto& sun_transform = sun.transform();
  sun_transform.rotation = sbx::math::quaternion::look_at(sbx::math::vector3f{-0.4f, -1.0f, -0.5f});

  auto& sun_light = sun.add_component<sbx::scenes::directional_light>();
  sun_light.intensity = 3.0f;

  scene.set_primary_light(sun);

  sbx::scenes::scene_serializer::save(scene, "scenes/demo_scene.yaml");
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

  auto& flight_helmet_transform = _flight_helmet.transform();
  flight_helmet_transform.rotation = sbx::math::quaternion{sbx::math::vector3f{0.0f, 1.0f, 0.0f}, _rotation};
}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

} // namespace demo
