// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <demo/application.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/math/matrix4x4.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/input.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/types.hpp>

#include <libsbx/assets/assets_module.hpp>

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

  auto& assets_module = sbx::core::engine::get_module<sbx::assets::assets_module>();

  const auto texture = assets_module.load_texture("demo/assets/models/duck/textures/albedo.png");
  const auto mesh = assets_module.load_mesh("demo/assets/models/duck/duck.gltf");

  auto material = assets_module.create_material(sbx::assets::material::create_info{
    .albedo = texture
  });

  auto& render_module = sbx::core::engine::get_module<sbx::render::render_module>();

  auto& scenes_module = sbx::core::engine::get_module<sbx::scenes::scenes_module>();
  auto& scene = scenes_module.active_scene();

  const auto& bounds = mesh->bounds();
  const auto center = bounds.center();
  const auto radius = std::max(bounds.diagonal_length() * 0.5f, 0.001f);
  const auto fov = 60.0f;
  const auto distance = radius / std::sin(sbx::math::to_radians(sbx::math::degree{fov}).value() * 0.5f) * 2.0f;

  _duck = scene.create_node();
  _duck.transform().position = center * -1.0f;

  auto& renderer = _duck.add_component<sbx::scenes::mesh_renderer>();
  renderer.mesh = mesh;
  renderer.material = material;

  auto camera = scene.create_node();
  camera.transform().position = sbx::math::vector3f{0.0f, 0.0f, distance};

  auto& camera_component = camera.add_component<sbx::scenes::camera>();
  camera_component.fov_degrees = fov;
  camera_component.near_plane = std::max(0.01f, distance - radius * 2.0f);
  camera_component.far_plane = distance + radius * 2.0f;

  scene.set_active_camera(camera);
}

auto application::update() -> void {
  using namespace sbx::units::literals;

  if (sbx::platform::input::is_key_pressed(sbx::platform::key::escape)) {
    sbx::core::engine::quit();
  }

  _rotation += sbx::math::degree{90.0f} * sbx::core::engine::delta_time();
  _duck.transform().rotation = sbx::math::quaternion{sbx::math::vector3f{0.0f, 1.0f, 0.0f}, _rotation};
}

auto application::fixed_update() -> void {

}

auto application::is_paused() const -> bool {
  return _is_paused;
}

} // namespace demo
