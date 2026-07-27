// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_RESOURCES_BUFFER_HPP_
#define LIBSBX_GRAPHICS_RESOURCES_BUFFER_HPP_

#include <cstdint>
#include <span>
#include <string>

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/types.hpp>
#include <libsbx/graphics/resources/resource_handle.hpp>

namespace sbx::graphics {

/**
 * @brief A single buffer type covering vertex, index, uniform, storage, indirect and staging use.
 *
 * There are no subclasses. What a buffer is for is expressed by @ref buffer_create_info::usage,
 * and where its memory lives by @ref buffer_create_info::memory.
 *
 * If the usage flags include `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT` the device address is
 * queried on construction and available through @ref address. Passing that address in a push
 * constant is the intended way to reach buffer contents from a shader, which is why there is no
 * descriptor binding involved anywhere in this type.
 */
class buffer : public utility::noncopyable {

public:

  using handle_type = VkBuffer;
  using size_type = VkDeviceSize;
  using address_type = VkDeviceAddress;

  struct create_info {
    size_type size{};
    buffer_usage usage{};
    memory_usage memory{memory_usage::device_local};
    std::string name{"Buffer"};
  }; // struct create_info

  explicit buffer(const create_info& create_info);

  ~buffer();

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

  [[nodiscard]] auto size() const noexcept -> size_type {
    return _size;
  }

  [[nodiscard]] auto usage() const noexcept -> buffer_usage {
    return _usage;
  }

  /**
   * @brief The device address, or zero if the buffer was not created with
   * `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`.
   */
  [[nodiscard]] auto address() const noexcept -> address_type {
    return _address;
  }

  /**
   * @brief The persistent mapping, or null for device local buffers.
   */
  [[nodiscard]] auto mapped() const noexcept -> void* {
    return _mapped;
  }

  [[nodiscard]] auto is_mapped() const noexcept -> bool {
    return _mapped != nullptr;
  }

  /**
   * @brief Copies @p size bytes into the mapping and flushes the affected range.
   *
   * Only valid on a host visible buffer. Device local buffers are filled through the upload context.
   */
  auto write(const void* data, size_type size, size_type offset = 0u) -> void;

  template<typename Type>
  auto write(const std::span<const Type> data, const size_type offset = 0u) -> void {
    write(data.data(), static_cast<size_type>(data.size_bytes()), offset);
  }

private:

  handle_type _handle{};
  VmaAllocation _allocation{};

  size_type _size{};
  buffer_usage _usage{};
  address_type _address{};

  void* _mapped{};

}; // class buffer

using buffer_handle = resource_handle<buffer>;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_RESOURCES_BUFFER_HPP_
