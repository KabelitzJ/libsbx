// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/target.hpp>
#include <libsbx/utility/assert.hpp>

namespace sbx::core {

engine* engine::_instance{nullptr};

engine::engine(const engine_config& config)
: _config{config} {
  utility::assert_that(_instance == nullptr, "Engine instance already exists");

  _instance = this;

  // Resolved before any module constructs, since some modules (e.g. graphics_module) build project-relative state in their own constructor.
  if (_config.project.has_value()) {
    const auto& initial = *_config.project;

    set_project(core::project::open_or_create(initial.root, initial.name));
  }
}

engine::~engine() {
  _instance = nullptr;
}

auto engine::delta_time() -> units::seconds {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");

  return _instance->_delta_time;
}

auto engine::fixed_delta_time() -> units::seconds {
  return units::seconds{1.0f / 60.0f};
}

auto engine::time() -> units::seconds {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");

  return _instance->_time;
}

auto engine::config() -> const engine_config& {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");

  return _instance->_config;
}

auto engine::quit() -> void {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");

  _instance->_is_running = false;
}

auto engine::projects() -> const std::deque<core::project>& {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");

  return _instance->_projects;
}

auto engine::has_project() -> bool {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");

  return _instance->_active_project.has_value();
}

auto engine::project() -> core::project& {
  utility::assert_that(_instance != nullptr, "Engine instance does not exist");
  utility::assert_that(_instance->_active_project.has_value(), "No active project — a project is required");

  return _instance->_projects[*_instance->_active_project];
}

} // namespace sbx::core
