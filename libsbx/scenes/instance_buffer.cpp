// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/instance_buffer.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/scenes/scenes_module.hpp>

namespace sbx::scenes {

instance_buffer::instance_buffer(std::span<const instance_data> instances)
: _count{static_cast<std::uint32_t>(instances.size())} {
  if (instances.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  // Device-local, since the cull pass reads every instance every frame; the data arrives through a staging copy the next frame records before any pass.
  _buffer = graphics_module.resource_registry().emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = instances.size_bytes(),
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage | graphics::buffer_usage::transfer_destination,
    .memory = graphics::memory_usage::device_local,
    .name = "Instances"
  });

  graphics_module.upload_context().stage_buffer(_buffer, std::as_bytes(instances));
}

instance_buffer::~instance_buffer() {
  if (!_buffer.is_valid()) {
    return;
  }

  core::engine::get_module<scenes_module>().release_instance_buffer(_buffer);
}

} // namespace sbx::scenes
