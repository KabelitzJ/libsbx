// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/utility/profiler.hpp>

namespace sbx::scenes {

scenes_module::scenes_module() {

}

scenes_module::~scenes_module() { 

}

auto scenes_module::late_update() -> void {
  SBX_PROFILE_SCOPE("scenes_module::late_update");

  _simulation_time += simulation_delta_time();

  _scene.update();
}

} // namespace sbx::scenes
