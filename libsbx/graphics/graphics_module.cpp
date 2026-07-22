// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/graphics/profiler.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

static auto _device_type_name(const VkPhysicalDeviceType type) -> std::string_view {
  switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
    case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
    default: return "other";
  }
}

graphics_module::graphics_module()
: _instance{},
  _physical_device{_instance},
  _logical_device{_physical_device},
  _allocator{_instance, _physical_device, _logical_device},
  _surface{_instance, _physical_device, _logical_device},
  _command_pools{},
  _resource_registry{},
  _bindless_table{_physical_device, _logical_device},
  _frame_context{},
  _upload_context{} {
  const auto& properties = _physical_device.properties();

  utility::logger<"graphics">::info("Device: {} ({})", std::string_view{properties.deviceName}, _device_type_name(properties.deviceType));
  utility::logger<"graphics">::info("Api version: {}.{}.{}", VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion));
  utility::logger<"graphics">::info("Device local memory: {} MiB", _physical_device.device_local_memory() / (1024u * 1024u));
  utility::logger<"graphics">::info("Queue families: ");
  utility::logger<"graphics">::info("  Graphics: {}", _logical_device.queue<queue::type::graphics>().family());
  utility::logger<"graphics">::info("  Present: {}", _logical_device.queue<queue::type::present>().family());
  utility::logger<"graphics">::info("  Compute: {}", _logical_device.queue<queue::type::compute>().family());
  utility::logger<"graphics">::info("  Transfer: {}", _logical_device.queue<queue::type::transfer>().family());

  SBX_PROFILE_GPU_CONTEXT_CREATE(queue::type::graphics, "graphics", _instance, _physical_device, _logical_device);
}

graphics_module::~graphics_module() {
  _logical_device.wait_idle();

  SBX_PROFILE_GPU_CONTEXT_DESTROY();
}

auto graphics_module::command_pool(const queue::type type, const std::thread::id& thread_id) -> const std::shared_ptr<graphics::command_pool>& {
  const auto key = command_pool_key{type, thread_id};

  if (auto entry = _command_pools.find(key); entry != _command_pools.end()) {
    return entry->second;
  }

  return _command_pools.insert({key, std::make_shared<graphics::command_pool>(type)}).first->second;
}

} // namespace sbx::graphics
