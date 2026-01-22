// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_ALLOCATOR_HPP_
#define LIBSBX_GRAPHICS_DEVICES_ALLOCATOR_HPP_

#include <vk_mem_alloc.h>

#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>

namespace sbx::graphics {

class allocator {

public:

  using handle_type = VmaAllocator;

  allocator(const instance& instance, const physical_device& physical_device, const logical_device& logical_device);

  allocator(const allocator& other) = delete;

  allocator(allocator&& other) = delete;

  ~allocator();

  auto operator=(const allocator& other) -> allocator& = delete;
  
  auto operator=(allocator&& other) -> allocator& = delete;

  auto handle() const -> handle_type;

  operator handle_type() const noexcept {
    return handle();
  }

private:

  handle_type _handle;

}; // class allocator

}; // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_ALLOCATOR_HPP_