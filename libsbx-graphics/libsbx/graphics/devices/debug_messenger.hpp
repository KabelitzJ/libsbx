// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_DEBUG_MESSENGER_HPP_
#define LIBSBX_GRAPHICS_DEVICES_DEBUG_MESSENGER_HPP_

#include <cstdint>

#include <vulkan/vulkan.hpp>

#include <libsbx/graphics/devices/instance.hpp>

namespace sbx::graphics {

auto set_debug_name(const VkObjectType object_type, const std::uint64_t object_handle, const std::string& name) -> void;

class debug_messenger {

public:

  debug_messenger() = delete;

  ~debug_messenger() = default;

  [[nodiscard]] static auto create(const instance& target, const VkAllocationCallbacks* allocator = nullptr) -> VkResult;

  static auto destroy(const instance& target, const VkAllocationCallbacks* allocator = nullptr) -> void;

  static auto create_info() -> VkDebugUtilsMessengerCreateInfoEXT*;

private:

  static VKAPI_ATTR auto VKAPI_CALL _debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, [[maybe_unused]] void* user_data) -> VkBool32;

  inline static VkDebugUtilsMessengerEXT _handle{};

}; // class debug_messenger

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_DEBUG_MESSENGER_HPP_
