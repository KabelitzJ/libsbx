// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef RUNTIME_APPLICATION_HPP_
#define RUNTIME_APPLICATION_HPP_

#include <stb_image.h>

#include <libsbx/units/units.hpp>

#include <libsbx/math/angle.hpp>

#include <libsbx/core/application.hpp>

#include <libsbx/scenes/scene.hpp>

#include <runtime/fly_camera.hpp>

namespace runtime {

class application : public sbx::core::application {

public:

  application();

  ~application() override;

  auto update() -> void override;

  auto fixed_update() -> void override;

  auto is_paused() const -> bool override;

private:

  bool _is_paused;

  sbx::scenes::node _camera;
  fly_camera _camera_controller;

}; // class application

} // namespace runtime

#endif // RUNTIME_APPLICATION_HPP_
