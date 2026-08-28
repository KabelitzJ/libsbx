// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_APPLICATION_HPP_
#define EDITOR_APPLICATION_HPP_

#include <stb_image.h>

#include <libsbx/units/units.hpp>

#include <libsbx/math/angle.hpp>

#include <libsbx/core/application.hpp>

#include <libsbx/scenes/scene.hpp>

#include <editor/fly_camera.hpp>

namespace editor {

class application : public sbx::core::application {

public:

  application();

  ~application() override;

  auto update() -> void override;

  auto fixed_update() -> void override;

  auto is_paused() const -> bool override;

private:

  bool _is_paused;

  sbx::units::seconds _time;
  std::uint32_t _fps;

  sbx::math::angle _rotation;

  sbx::scenes::node _camera;

  fly_camera _camera_controller;

  // Click-to-engage: right-mouse-down only starts driving the camera while the Viewport panel is
  // hovered; once engaged it keeps driving until right-mouse is released, even off-panel.
  bool _camera_is_engaged{false};

}; // class application

} // namespace editor

#endif // EDITOR_APPLICATION_HPP_
