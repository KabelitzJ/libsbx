// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/instance.hpp>

#include <cstring>
#include <vector>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/target.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/platform_module.hpp>

namespace sbx::graphics {

#if defined(SBX_BUILD_TYPE_DEBUG)

static VKAPI_ATTR auto VKAPI_CALL _debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, [[maybe_unused]] void* user_data) -> VkBool32 {
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    utility::logger<"vulkan">::error("{}", data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    utility::logger<"vulkan">::warn("{}", data->pMessage);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    utility::logger<"vulkan">::info("{}", data->pMessage);
  } else {
    utility::logger<"vulkan">::trace("{}", data->pMessage);
  }

  return VK_FALSE;
}

#endif // SBX_BUILD_TYPE_DEBUG

static constexpr auto validation_layer_name = "VK_LAYER_KHRONOS_validation";

static auto _is_validation_layer_available() -> bool {
  auto count = std::uint32_t{0};
  vkEnumerateInstanceLayerProperties(&count, nullptr);

  auto layers = std::vector<VkLayerProperties>{count};
  vkEnumerateInstanceLayerProperties(&count, layers.data());

  for (const auto& layer : layers) {
    if (std::strcmp(layer.layerName, validation_layer_name) == 0) {
      return true;
    }
  }

  return false;
}

instance::instance() {
  auto instance_version = std::uint32_t{0};
  validate(vkEnumerateInstanceVersion(&instance_version), "vkEnumerateInstanceVersion");

  if (instance_version < VK_API_VERSION_1_3) {
    throw utility::runtime_error{"Vulkan 1.3 required, loader reports {}.{}", VK_API_VERSION_MAJOR(instance_version), VK_API_VERSION_MINOR(instance_version)};
  }

  auto application_info = VkApplicationInfo{};
  application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application_info.pApplicationName = "libsbx";
  application_info.applicationVersion = VK_MAKE_VERSION(0u, 1u, 0u);
  application_info.pEngineName = "libsbx";
  application_info.engineVersion = VK_MAKE_VERSION(0u, 1u, 0u);
  application_info.apiVersion = VK_API_VERSION_1_3;

  auto& platform_module = core::engine::get_module<platform::platform_module>();

  auto extensions = platform_module.required_instance_extensions();

  auto layers = std::vector<const char*>{};

  if constexpr (utility::is_build_type_debug_v) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (_is_validation_layer_available()) {
      layers.push_back(validation_layer_name);
    } else {
      utility::logger<"graphics">::warn("Validation layer '{}' not available", validation_layer_name);
    }
  }

  auto create_info = VkInstanceCreateInfo{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &application_info;
  create_info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();
  create_info.enabledLayerCount = static_cast<std::uint32_t>(layers.size());
  create_info.ppEnabledLayerNames = layers.data();

  validate(vkCreateInstance(&create_info, nullptr, &_handle), "vkCreateInstance");

#if defined(SBX_BUILD_TYPE_DEBUG)
  _create_debug_messenger();
#endif
}

instance::~instance() {
#if defined(SBX_BUILD_TYPE_DEBUG)
  _destroy_debug_messenger();
#endif

  vkDestroyInstance(_handle, nullptr);
}

#if defined(SBX_BUILD_TYPE_DEBUG)

auto instance::_create_debug_messenger() -> void {
  auto* create_function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(_handle, "vkCreateDebugUtilsMessengerEXT"));

  _destroy_debug_messenger_function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(_handle, "vkDestroyDebugUtilsMessengerEXT"));

  if (!create_function || !_destroy_debug_messenger_function) {
    utility::logger<"graphics">::warn("Debug utils extension functions not available");

    return;
  }

  auto create_info = VkDebugUtilsMessengerCreateInfoEXT{};
  create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  create_info.pfnUserCallback = _debug_callback;

  validate(create_function(_handle, &create_info, nullptr, &_debug_messenger), "vkCreateDebugUtilsMessengerEXT");
}

auto instance::_destroy_debug_messenger() -> void {
  if (_debug_messenger && _destroy_debug_messenger_function) {
    _destroy_debug_messenger_function(_handle, _debug_messenger, nullptr);

    _debug_messenger = nullptr;
  }
}

#endif // SBX_BUILD_TYPE_DEBUG

} // namespace sbx::graphics
