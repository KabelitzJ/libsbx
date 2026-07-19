// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_

#include <cstdint>
#include <string>

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/target.hpp>

#include <libsbx/graphics/vulkan.hpp>

#include <libsbx/graphics/devices/physical_device.hpp>

namespace sbx::graphics {

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
   * @brief Dedicated async compute queue; falls back to the graphics family.
   */
  [[nodiscard]] auto compute_queue() const noexcept -> const queue& {
    return _compute_queue;
  }

  /**
   * @brief Dedicated transfer queue; falls back to the graphics family.
   */
  [[nodiscard]] auto transfer_queue() const noexcept -> const queue& {
    return _transfer_queue;
  }

  auto wait_idle() const -> void;

  /**
   * @brief Attaches a name to a vulkan object (visible in validation messages and debuggers like RenderDoc). No-op in release builds.
   */
#if defined(SBX_BUILD_TYPE_DEBUG)
  auto set_debug_name(VkObjectType object_type, std::uint64_t object_handle, const std::string& name) const -> void;
#else
  auto set_debug_name([[maybe_unused]] VkObjectType object_type, [[maybe_unused]] std::uint64_t object_handle, [[maybe_unused]] const std::string& name) const -> void { }
#endif // SBX_BUILD_TYPE_DEBUG

private:

  handle_type _handle{};

  queue _graphics_queue{};
  queue _compute_queue{};
  queue _transfer_queue{};

#if defined(SBX_BUILD_TYPE_DEBUG)

  PFN_vkSetDebugUtilsObjectNameEXT _set_object_name_function{};

#endif // SBX_BUILD_TYPE_DEBUG

}; // class logical_device

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_LOGICAL_DEVICE_HPP_
