// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_

#include <unordered_map>
#include <optional>
#include <cinttypes>

#include <vulkan/vulkan.hpp>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/enum.hpp>

#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>

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

  queue(queue&& other) noexcept = default;

  ~queue() = default;

  auto operator=(queue&& other) noexcept -> queue& = default;

  auto handle() const noexcept -> const VkQueue&;

  operator const VkQueue&() const noexcept;

  auto family() const noexcept -> std::uint32_t;

  auto wait_idle() const -> void;

private:

  queue()
  : _handle{VK_NULL_HANDLE},
    _family{0xFFFFFFFF} { }

  queue(const VkQueue& handle, std::uint32_t family)
  : _handle{handle},
    _family{family} { }

  VkQueue _handle{};
  std::uint32_t _family{};

}; // class queue

class logical_device : public utility::noncopyable {

public:

  logical_device(const physical_device& physical_device);

  ~logical_device();

  auto handle() const noexcept -> const VkDevice&;

  operator const VkDevice&() const noexcept;

  auto enabled_features() const -> const physical_device::device_features&;

  template<queue::type Type>
  auto queue() const -> const graphics::queue& {
    return _queues.at(utility::to_underlying(Type));
  }

  auto queue(const queue::type type) const -> const graphics::queue& {
    return _queues.at(utility::to_underlying(type));
  }

  auto wait_idle() const -> void;

private:

  struct queue_family_indices {
    std::optional<std::uint32_t> graphics{};
    std::optional<std::uint32_t> present{};
    std::optional<std::uint32_t> compute{};
    std::optional<std::uint32_t> transfer{};
  }; // struct queue_family_indices

  template<queue::type Type>
  auto _get_queue(const std::uint32_t queue_family_index, std::uint32_t index = 0u) -> void {
    auto handle = VkQueue{};

    vkGetDeviceQueue(_handle, queue_family_index, index, &handle);

    _queues.at(utility::to_underlying(Type)) = graphics::queue{handle, queue_family_index};
  }

  auto _get_queue_family_indices(const physical_device& physical_device) const -> queue_family_indices;

  auto _get_enabled_features(const physical_device& physical_device) const -> physical_device::device_features;

  auto _create_logical_device(const physical_device& physical_device) -> void;

  VkDevice _handle{};
  physical_device::device_features _enabled_features{};
  // std::unordered_map<queue::type, graphics::queue> _queues{};
  std::array<graphics::queue, 4u> _queues{};

}; // class logical_device

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_

