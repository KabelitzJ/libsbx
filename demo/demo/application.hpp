// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef DEMO_APPLICATION_HPP_
#define DEMO_APPLICATION_HPP_

#include <libsbx/core/application.hpp>

#include <libsbx/units/units.hpp>

namespace demo {

class application : public sbx::core::application {

public:

  application();

  ~application() override = default;

  auto update() -> void override;

  auto fixed_update() -> void override;

  auto is_paused() const -> bool override;

private:

  bool _is_paused;

  sbx::units::seconds _time;
  std::uint32_t _fps;

}; // class application

} // namespace demo

#endif // DEMO_APPLICATION_HPP_
