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

  auto& registry = core::engine::get_module<graphics::graphics_module>().resource_registry();

  // ponytail: host-visible, read straight over the bus by the cull pass each frame; a device-local
  // copy via upload_context if instance counts ever make that read show up in GPU time.
  _buffer = registry.emplace<graphics::buffer>(graphics::buffer::create_info{
    .size = instances.size_bytes(),
    .usage = graphics::buffer_usage::device_address | graphics::buffer_usage::storage,
    .memory = graphics::memory_usage::host_write,
    .name = "Instances"
  });

  registry.get<graphics::buffer>(_buffer).write(instances.data(), instances.size_bytes());
}

instance_buffer::~instance_buffer() {
  if (!_buffer.is_valid()) {
    return;
  }

  core::engine::get_module<scenes_module>().release_instance_buffer(_buffer);
}

} // namespace sbx::scenes
