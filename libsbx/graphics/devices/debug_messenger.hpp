// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_DEVICES_DEBUG_MESSENGER_HPP_
#define LIBSBX_GRAPHICS_DEVICES_DEBUG_MESSENGER_HPP_

#include <cstdint>
#include <type_traits>

#include <vulkan/vulkan.h>

#include <libsbx/utility/target.hpp>

#include <libsbx/graphics/devices/instance.hpp>

namespace sbx::graphics {

class debug_messenger {

public:

  using handle_type = VkDebugUtilsMessengerEXT;

  debug_messenger() = delete;

  ~debug_messenger() = default;

  [[nodiscard]] static auto create(const instance& target, const VkAllocationCallbacks* allocator = nullptr) -> VkResult;

  static auto destroy(const instance& target, const VkAllocationCallbacks* allocator = nullptr) -> void;

  static auto create_info() -> VkDebugUtilsMessengerCreateInfoEXT*;

private:

  static VKAPI_ATTR auto VKAPI_CALL _debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, [[maybe_unused]] void* user_data) -> VkBool32;

  inline static handle_type _handle{};

}; // class debug_messenger

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_DEBUG_MESSENGER_HPP_
