// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LAUNCHER_APPLICATION_HPP_
#define LAUNCHER_APPLICATION_HPP_

#include <libsbx/core/application.hpp>

namespace launcher {

/**
 * @brief The launcher has no simulation of its own — everything it does (recent projects,
 * New/Open Project, spawning the editor) is driven by launcher_module's ui_layer, once per frame
 * between NewFrame() and Render(). This exists only because basic_engine::run<Application>()
 * requires a concrete core::application; every hook is a no-op.
 */
class application final : public sbx::core::application {

public:

  auto update() -> void override { }

  auto fixed_update() -> void override { }

  auto is_paused() const -> bool override {
    return false;
  }

}; // class application

} // namespace launcher

#endif // LAUNCHER_APPLICATION_HPP_
