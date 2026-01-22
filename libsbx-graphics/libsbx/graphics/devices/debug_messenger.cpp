// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/debug_messenger.hpp>

#include <libsbx/utility/target.hpp>
#include <libsbx/utility/logger.hpp>

namespace sbx::graphics {

auto debug_messenger::create(const instance& target, const VkAllocationCallbacks* allocator) -> VkResult {
  if constexpr (utility::build_configuration_v == utility::build_configuration::debug) {
    auto* function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(target.handle(), "vkCreateDebugUtilsMessengerEXT"));

    if (function) {
      return function(target.handle(), create_info(), allocator, &_handle);
    }

    return VK_ERROR_EXTENSION_NOT_PRESENT;
  } else {
    return VK_SUCCESS;
  }
}

 auto debug_messenger::destroy(const instance& target, const VkAllocationCallbacks* allocator) -> void {
  if constexpr (utility::build_configuration_v == utility::build_configuration::debug) {
    auto* function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(target.handle(), "vkDestroyDebugUtilsMessengerEXT"));

    if (function) {
      function(target.handle(), _handle, allocator);
    }
  }
}

auto debug_messenger::create_info() -> VkDebugUtilsMessengerCreateInfoEXT* {
  if constexpr (utility::build_configuration_v == utility::build_configuration::debug) {
    static auto validation_feature_enables = std::array<VkValidationFeatureEnableEXT, 4u>{
      VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
      VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
      VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
      VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT
    };

    static auto validation_features = VkValidationFeaturesEXT{};
    validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validation_features.enabledValidationFeatureCount = static_cast<std::uint32_t>(validation_feature_enables.size());
    validation_features.pEnabledValidationFeatures = validation_feature_enables.data();

    static auto create_info = VkDebugUtilsMessengerCreateInfoEXT{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.pNext = &validation_features;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = _debug_callback;

    return &create_info;
  } else {
    return nullptr;
  }
}

static auto message_type_to_string(const VkDebugUtilsMessageTypeFlagsEXT message_type) -> std::string_view {
  switch (message_type) {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
      return "General";
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
      return "Validation";
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
      return "Performance";
    default:
      return "Unknown";
  }
}

VKAPI_ATTR auto VKAPI_CALL debug_messenger::_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, [[maybe_unused]] void* user_data) -> VkBool32 {
  const auto message_type_str = message_type_to_string(message_type);

  // if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
  //   utility::logger<"graphics">::info("{}: {}", message_type_str, callback_data->pMessage);
  // } else 
  
  if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    utility::logger<"graphics">::warn("{}: {}", message_type_str, callback_data->pMessage);
  } else if (message_severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    utility::logger<"graphics">::error("{}: {}", message_type_str, callback_data->pMessage);
    std::terminate();
  }

  return VK_FALSE;
}

} // namespace sbx::graphics
