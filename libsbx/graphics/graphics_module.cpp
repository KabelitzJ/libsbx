// SPDX-License-Identifier: MIT
#include <libsbx/graphics/graphics_module.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>

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
  _is_framebuffer_resized{true} {
  const auto& properties = _physical_device.properties();

  utility::logger<"graphics">::info("Device: {} ({})", std::string_view{properties.deviceName}, _device_type_name(properties.deviceType));
  utility::logger<"graphics">::info("Api version: {}.{}.{}", VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion));
  utility::logger<"graphics">::info("Device local memory: {} MiB", _physical_device.device_local_memory() / (1024u * 1024u));
  utility::logger<"graphics">::info("Queue families: graphics {}, compute {}, transfer {}", _logical_device.graphics_queue().family, _logical_device.compute_queue().family, _logical_device.transfer_queue().family);

  auto& platform_module = core::engine::get_module<platform::platform_module>();

  auto& window = platform_module.window();

  window.on_framebuffer_resized() += [this]([[maybe_unused]] const platform::framebuffer_resized_event& event) {
    _is_framebuffer_resized = true;
  };
}

graphics_module::~graphics_module() {
  _logical_device.wait_idle();

  _swapchain.reset();
}

} // namespace sbx::graphics
