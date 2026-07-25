// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef DEMO_APPLICATION_HPP_
#define DEMO_APPLICATION_HPP_

#include <stb_image.h>

#include <libsbx/units/units.hpp>

#include <libsbx/math/angle.hpp>

#include <libsbx/core/application.hpp>

#include <libsbx/scenes/scene.hpp>

namespace demo {

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

  sbx::scenes::scene::node _duck;
  sbx::scenes::scene::node _damaged_helmet;
  sbx::scenes::scene::node _flight_helmet;

}; // class application

} // namespace demo

#endif // DEMO_APPLICATION_HPP_
