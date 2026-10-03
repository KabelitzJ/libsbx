// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scripting/script_resources.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::scripting {

script_resources::~script_resources() {
  clear();
}

auto script_resources::clear() -> void {
  // A command buffer can't be freed while the GPU may still execute it.
  for (auto& [id, state] : compute_commands) {
    if (state.fence) {
      state.fence->wait();
    }
  }

  compute_commands.clear();
  compute_shaders.clear();

  if (!compute_buffers.empty()) {
    auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
    const auto frame_index = graphics_module.frame_context().frame_index();

    for (const auto& [id, state] : compute_buffers) {
      graphics_module.resource_registry().retire(state.handle, frame_index);
    }

    compute_buffers.clear();
  }

  pending_geometries.clear();
  pending_instances.clear();
}

} // namespace sbx::scripting
