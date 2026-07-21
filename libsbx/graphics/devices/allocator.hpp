// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_DEVICES_ALLOCATOR_HPP_
#define LIBSBX_GRAPHICS_DEVICES_ALLOCATOR_HPP_

#include <libsbx/utility/noncopyable.hpp>

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>

namespace sbx::graphics {

class allocator : public utility::noncopyable {

public:

  using handle_type = VmaAllocator;

  allocator(const instance& instance, const physical_device& physical_device, const logical_device& logical_device);

  ~allocator();

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

private:

  handle_type _handle{};

}; // class allocator

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_ALLOCATOR_HPP_
