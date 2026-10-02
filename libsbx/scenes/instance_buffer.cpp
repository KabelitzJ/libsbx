// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/scenes/instance_buffer.hpp>

#include <mutex>
#include <vector>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::scenes {

namespace {

struct released_buffers {
  std::mutex mutex{};
  std::vector<graphics::buffer_handle> handles{};
}; // struct released_buffers

auto released() -> released_buffers& {
  static auto buffers = released_buffers{};
  return buffers;
}

} // namespace

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

  auto& queue = released();
  auto lock = std::lock_guard{queue.mutex};
  queue.handles.push_back(_buffer);
}

auto instance_buffer::collect_released() -> void {
  auto& queue = released();
  auto lock = std::lock_guard{queue.mutex};

  if (queue.handles.empty()) {
    return;
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();
  auto& registry = graphics_module.resource_registry();
  const auto frame_index = graphics_module.frame_context().frame_index();

  for (const auto handle : queue.handles) {
    registry.retire(handle, frame_index);
  }

  queue.handles.clear();
}

} // namespace sbx::scenes
