// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/resources/buffer.hpp>

#include <cstring>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/utility/assert.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

static auto _allocation_flags_for(const memory_usage memory) noexcept -> VmaAllocationCreateFlags {
  switch (memory) {
    case memory_usage::host_write: {
      return VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    case memory_usage::host_read: {
      return VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    default: {
      return VmaAllocationCreateFlags{0u};
    }
  }
}

static auto _memory_usage_for(const memory_usage memory) noexcept -> VmaMemoryUsage {
  return (memory == memory_usage::device_local) ? VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE : VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
}

buffer::buffer(const create_info& create_info)
: _size{create_info.size},
  _usage{create_info.usage} {
  utility::assert_that(create_info.size > 0u, "Cannot create a buffer with a size of zero");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
  const auto& allocator = graphics_module.allocator();

  auto buffer_info = VkBufferCreateInfo{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = create_info.size;
  buffer_info.usage = to_vk_enum<VkBufferUsageFlags>(create_info.usage);
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  auto allocation_create_info = VmaAllocationCreateInfo{};
  allocation_create_info.usage = _memory_usage_for(create_info.memory);
  allocation_create_info.flags = _allocation_flags_for(create_info.memory);

  auto allocation_info = VmaAllocationInfo{};

  validate(vmaCreateBuffer(allocator, &buffer_info, &allocation_create_info, &_handle, &_allocation, &allocation_info), "vmaCreateBuffer");

  _mapped = allocation_info.pMappedData;

  if (reflection::to_underlying(create_info.usage & buffer_usage::device_address) != 0) {
    auto address_info = VkBufferDeviceAddressInfo{};
    address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    address_info.buffer = _handle;

    _address = vkGetBufferDeviceAddress(logical_device, &address_info);
  }

  if (!create_info.name.empty()) {
    logical_device.set_debug_name(_handle, create_info.name);
  }
}

buffer::~buffer() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& allocator = graphics_module.allocator();

  vmaDestroyBuffer(allocator, _handle, _allocation);
}

auto buffer::write(const void* data, const size_type size, const size_type offset) -> void {
  utility::assert_that(_mapped != nullptr, "Cannot write to a buffer that is not host visible");
  utility::assert_that(offset + size <= _size, "Write exceeds the bounds of the buffer");

  std::memcpy(static_cast<std::byte*>(_mapped) + offset, data, size);

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& allocator = graphics_module.allocator();

  validate(vmaFlushAllocation(allocator, _allocation, offset, size), "vmaFlushAllocation");
}

} // namespace sbx::graphics
