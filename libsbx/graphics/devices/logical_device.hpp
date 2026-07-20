// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_

#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/target.hpp>

#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/object_type.hpp>

namespace sbx::graphics {

namespace detail {

template<typename Type>
struct object_type;

template<>
struct object_type<VkInstance> {
  inline static constexpr auto value = VK_OBJECT_TYPE_INSTANCE;
}; // struct object_type

template<>
struct object_type<VkPhysicalDevice> {
  inline static constexpr auto value = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
}; // struct object_type

template<>
struct object_type<VkDevice> {
  inline static constexpr auto value = VK_OBJECT_TYPE_DEVICE;
}; // struct object_type

template<typename Type>
constexpr auto object_type_v = object_type<Type>::value;

} // namespace detail

class logical_device : public utility::noncopyable {

public:

  using handle_type = VkDevice;

  struct queue {

    std::uint32_t family{};
    VkQueue handle{};

    operator VkQueue() const noexcept {
      return handle;
    }

  }; // struct queue

  explicit logical_device(const physical_device& physical_device);

  ~logical_device();

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

  /**
   * @brief Graphics + compute + present capable queue.
   */
  [[nodiscard]] auto graphics_queue() const noexcept -> const queue& {
    return _graphics_queue;
  }

  /**
   * @brief Dedicated async compute queue. Falls back to the graphics family.
   */
  [[nodiscard]] auto compute_queue() const noexcept -> const queue& {
    return _compute_queue;
  }

  /**
   * @brief Dedicated transfer queue. Falls back to the graphics family.
   */
  [[nodiscard]] auto transfer_queue() const noexcept -> const queue& {
    return _transfer_queue;
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
  
private:

  auto _set_debug_name(VkObjectType object_type, std::uint64_t object_handle, const std::string& name) const -> void;

  handle_type _handle{};

  queue _graphics_queue{};
  queue _compute_queue{};
  queue _transfer_queue{};

}; // class logical_device

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_
