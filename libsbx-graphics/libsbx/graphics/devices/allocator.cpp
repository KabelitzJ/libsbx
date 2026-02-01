// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/allocator.hpp>

#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <fmt/format.h>

namespace sbx::graphics {

allocator::allocator(const instance& instance, const physical_device& physical_device, const logical_device& logical_device) {
  auto vulkan_functions = VmaVulkanFunctions{};
  vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  auto allocator_info = VmaAllocatorCreateInfo{};
  allocator_info.physicalDevice = physical_device;
  allocator_info.device = logical_device;
  allocator_info.instance = instance;
  allocator_info.pVulkanFunctions = &vulkan_functions;
  allocator_info.vulkanApiVersion = VK_API_VERSION_1_4;
  allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  vmaCreateAllocator(&allocator_info, &_handle);
}

allocator::~allocator() {
  vmaDestroyAllocator(_handle);
}

auto allocator::handle() const -> handle_type {
  return _handle;
}

}; // namespace sbx::graphics
