// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/scenes_module.hpp>

#include <libsbx/utility/profiler.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scene_serializer.hpp>

namespace sbx::scenes {

scenes_module::scenes_module() {

}

scenes_module::~scenes_module() { 

}

auto scenes_module::late_update() -> void {
  SBX_PROFILE_SCOPE("scenes_module::late_update");

  _simulation_time += simulation_delta_time();

  _scene.update();

  // Resyncs any prefab_instance whose source has been edited since this scene last saw it — see
  // scene_serializer::sync_prefab_instances. Runs every frame, editor and runtime alike, same as
  // the transform update above.
  scene_serializer::sync_prefab_instances(_scene);
}

auto scenes_module::release_instance_buffer(graphics::buffer_handle handle) -> void {
  auto lock = std::lock_guard{_released_instance_buffers_mutex};
  _released_instance_buffers.push_back(handle);
}

auto scenes_module::collect_released_instance_buffers() -> void {
  auto lock = std::lock_guard{_released_instance_buffers_mutex};

  if (_released_instance_buffers.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  const auto frame_index = graphics_module.frame_context().frame_index();

  for (const auto handle : _released_instance_buffers) {
    registry.retire(handle, frame_index);
  }

  _released_instance_buffers.clear();
}

} // namespace sbx::scenes
