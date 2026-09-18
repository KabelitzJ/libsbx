// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_DEVICES_PHYSICAL_DEVICE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_PHYSICAL_DEVICE_HPP_

#include <string_view>
#include <vector>
#include <cstdint>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/devices/instance.hpp>

namespace sbx::graphics {

/** @brief "discrete"/"integrated"/"virtual"/"cpu"/"other" -- also used for the physical_device selection log line. */
[[nodiscard]] auto device_type_name(VkPhysicalDeviceType type) -> std::string_view;

/** @brief PCI vendor id (VkPhysicalDeviceProperties::vendorID) to a human-readable name, e.g. "NVIDIA". "Unknown" for anything not in the small known-vendor table. */
[[nodiscard]] auto vendor_name(std::uint32_t vendor_id) -> std::string_view;

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

  /**
   * @brief Total device-local memory in bytes.
   */
  [[nodiscard]] auto device_local_memory() const noexcept -> VkDeviceSize;

private:

  handle_type _handle{};

  VkPhysicalDeviceProperties _properties{};
  VkPhysicalDeviceMemoryProperties _memory_properties{};

}; // class physical_device

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_PHYSICAL_DEVICE_HPP_
