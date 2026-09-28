// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

#include <vulkan/vulkan.h>

#include <libsbx/reflection/enum.hpp>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/target.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/object_type.hpp>
#include <libsbx/graphics/devices/features.hpp>
#include <libsbx/graphics/devices/surface.hpp>

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

  /**
   * @brief Hold around every vkQueueSubmit/vkQueuePresentKHR on this queue: Vulkan requires external
   * synchronization per VkQueue, and the render thread and the main thread (script compute, IBL
   * bakes, storage images) both submit. Queue types that alias one VkQueue (e.g. compute on a device
   * without a dedicated compute family) share one mutex.
   */
  [[nodiscard]] auto lock() const -> std::unique_lock<std::mutex>;

private:

  queue()
  : _handle{VK_NULL_HANDLE},
    _family{0xFFFFFFFF} { }

  queue(const VkQueue& handle, std::uint32_t family, memory::observer_ptr<std::mutex> mutex)
  : _handle{handle},
    _family{family},
    _mutex{mutex} { }

  handle_type _handle{};
  std::uint32_t _family{};
  // Owned by logical_device (shared by aliasing queue types); mutable because locking is logically
  // const, same as a mutable mutex member, and observer_ptr propagates const to the pointee.
  mutable memory::observer_ptr<std::mutex> _mutex{};

}; // class queue

class logical_device : public utility::noncopyable {

public:

  using handle_type = VkDevice;

  logical_device(const physical_device& physical_device, const surface& surface);

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
    return _queues.at(std::to_underlying(Type));
  }

  auto queue(const queue::type type) const -> const graphics::queue& {
    return _queues.at(std::to_underlying(type));
  }

  /** @brief The features actually enabled at device creation (required ∪ (optional ∩ available)) -- see features::enabled. Callers that want to use an optional feature (e.g. pipeline statistics queries) must check here first; requesting it as required/optional alone doesn't guarantee it was granted. */
  [[nodiscard]] auto enabled_features() const noexcept -> const features& {
    return _enabled_features;
  }

private:

  template<queue::type Type>
  auto _get_queue(const std::uint32_t queue_family_index, std::uint32_t index = 0u) -> void {
    auto handle = VkQueue{};

    vkGetDeviceQueue(_handle, queue_family_index, index, &handle);

    auto mutex = memory::make_observer(_queue_mutexes.at(std::to_underlying(Type)));

    for (const auto& existing : _queues) {
      if (existing._handle == handle) {
        mutex = existing._mutex;
        break;
      }
    }

    _queues.at(std::to_underlying(Type)) = graphics::queue{handle, queue_family_index, mutex};
  }

  auto _set_debug_name(VkObjectType object_type, std::uint64_t object_handle, const std::string& name) const -> void;

  handle_type _handle{};

  // One per queue type; aliasing queue types point at the first one's (see _get_queue). Locked all
  // together by wait_idle, since vkDeviceWaitIdle needs every queue externally synchronized.
  mutable std::array<std::mutex, 4u> _queue_mutexes{};

  std::array<graphics::queue, 4u> _queues{};

  graphics::features _enabled_features{};

}; // class logical_device

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_
