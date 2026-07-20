// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_

#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/target.hpp>
#include <libsbx/utility/enum.hpp>

#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/object_type.hpp>

namespace sbx::graphics {

class queue : public utility::noncopyable {

  friend class logical_device;

public:

  enum class type : std::uint32_t {
    graphics = 0,
    present = 1,
    compute = 2,
    transfer = 3
  }; // enum class type

  using handle_type = VkQueue;

  queue(queue&& other) noexcept = default;

  ~queue() = default;

  auto operator=(queue&& other) noexcept -> queue& = default;

  auto handle() const noexcept -> handle_type;

  operator handle_type() const noexcept;

  auto family() const noexcept -> std::uint32_t;

  auto wait_idle() const -> void;

private:

  queue()
  : _handle{VK_NULL_HANDLE},
    _family{0xFFFFFFFF} { }

  queue(const VkQueue& handle, std::uint32_t family)
  : _handle{handle},
    _family{family} { }

  handle_type _handle{};
  std::uint32_t _family{};

}; // class queue

class logical_device : public utility::noncopyable {

public:

  using handle_type = VkDevice;

  explicit logical_device(const physical_device& physical_device);

  ~logical_device();

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

  auto wait_idle() const -> void;

  /**
   * @brief Attaches a name to a vulkan object (visible in validation messages and debuggers like RenderDoc). No-op in release builds.
   */
  template<typename Handle>
  requires (named_object_type<Handle>)
  auto set_debug_name(const Handle handle, const std::string& name) const -> void {
    _set_debug_name(object_type_v<Handle>, reinterpret_cast<std::uint64_t>(handle), name);
  }

  template<queue::type Type>
  auto queue() const -> const graphics::queue& {
    return _queues.at(utility::to_underlying(Type));
  }

  auto queue(const queue::type type) const -> const graphics::queue& {
    return _queues.at(utility::to_underlying(type));
  }
  
private:

  template<queue::type Type>
  auto _get_queue(const std::uint32_t queue_family_index, std::uint32_t index = 0u) -> void {
    auto handle = VkQueue{};

    vkGetDeviceQueue(_handle, queue_family_index, index, &handle);

    _queues.at(utility::to_underlying(Type)) = graphics::queue{handle, queue_family_index};
  }

  auto _set_debug_name(VkObjectType object_type, std::uint64_t object_handle, const std::string& name) const -> void;

  handle_type _handle{};

  std::array<graphics::queue, 4u> _queues{};

}; // class logical_device

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_
