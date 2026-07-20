// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/allocator.hpp>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

allocator::allocator(const instance& instance, const physical_device& physical_device, const logical_device& logical_device) {
  auto create_info = VmaAllocatorCreateInfo{};
  create_info.instance = instance;
  create_info.physicalDevice = physical_device;
  create_info.device = logical_device;
  create_info.vulkanApiVersion = VK_API_VERSION_1_4;
  create_info.preferredLargeHeapBlockSize = 512 * 1024 * 1024; // 512 MiB
  create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  validate(vmaCreateAllocator(&create_info, &_handle), "vmaCreateAllocator");
}

allocator::~allocator() {
  vmaDestroyAllocator(_handle);
}

} // namespace sbx::graphics
