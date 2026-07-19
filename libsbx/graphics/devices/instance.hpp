// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_INSTANCE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_INSTANCE_HPP_

#include <libsbx/utility/noncopyable.hpp>
#include <libsbx/utility/target.hpp>

#include <libsbx/graphics/vulkan.hpp>

namespace sbx::graphics {

class instance : public utility::noncopyable {

public:

  using handle_type = VkInstance;

  instance();

  ~instance();

  [[nodiscard]] auto handle() const noexcept -> handle_type {
    return _handle;
  }

  operator handle_type() const noexcept {
    return _handle;
  }

private:

  handle_type _handle{};

#if defined(SBX_BUILD_TYPE_DEBUG)

  auto _create_debug_messenger() -> void;

  auto _destroy_debug_messenger() -> void;

  VkDebugUtilsMessengerEXT _debug_messenger{};
  PFN_vkDestroyDebugUtilsMessengerEXT _destroy_debug_messenger_function{};

#endif // SBX_BUILD_TYPE_DEBUG

}; // class instance

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_INSTANCE_HPP_
