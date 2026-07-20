// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_PHYSICAL_DEVICE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_PHYSICAL_DEVICE_HPP_

#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/devices/instance.hpp>

namespace sbx::graphics {

class physical_device : public utility::noncopyable {

public:

  using handle_type = VkPhysicalDevice;

  explicit physical_device(const instance& instance);

  ~physical_device() = default;

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

  [[nodiscard]] auto properties() const noexcept -> const VkPhysicalDeviceProperties& {
    return _properties;
  }

  [[nodiscard]] auto memory_properties() const noexcept -> const VkPhysicalDeviceMemoryProperties& {
    return _memory_properties;
  }

  [[nodiscard]] auto queue_families() const noexcept -> const std::vector<VkQueueFamilyProperties>& {
    return _queue_families;
  }

  /**
   * @brief Total device-local memory in bytes.
   */
  [[nodiscard]] auto device_local_memory() const noexcept -> VkDeviceSize;

private:

  handle_type _handle{};

  VkPhysicalDeviceProperties _properties{};
  VkPhysicalDeviceMemoryProperties _memory_properties{};
  std::vector<VkQueueFamilyProperties> _queue_families{};

}; // class physical_device

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_PHYSICAL_DEVICE_HPP_
