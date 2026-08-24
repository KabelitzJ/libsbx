// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/devices/extensions.hpp>

#include <vulkan/vulkan.h>

#include <libsbx/utility/iterator.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/target.hpp>
#include <libsbx/utility/logger.hpp>

namespace sbx::graphics {

auto extensions::device() -> std::vector<const char*> {
  auto required_extensions = std::vector<const char*>{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME
  };

#if defined(SBX_ENABLE_PROFILING)
  required_extensions.push_back(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME);
#endif

  return required_extensions;
}

auto extensions::instance() -> std::vector<const char*> {
  auto& platform_module = core::engine::get_module<platform::platform_module>();

  auto required_extensions = platform_module.required_instance_extensions();

  // required_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

  if constexpr (utility::is_build_type_debug_v) {
    required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    // required_extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
  }

  auto available_extention_count = std::uint32_t{0};

  vkEnumerateInstanceExtensionProperties(nullptr, &available_extention_count, nullptr);

  auto available_extensions = utility::make_vector<VkExtensionProperties>(available_extention_count);

  vkEnumerateInstanceExtensionProperties(nullptr, &available_extention_count, available_extensions.data());

  if constexpr (utility::is_build_type_debug_v) {
    utility::logger<"graphics">::debug("Available instance extension:");

    for (const auto& extension : available_extensions) {
      utility::logger<"graphics">::debug("\t{}", std::string_view{extension.extensionName});
    }
  }

  for (const auto* extension : required_extensions) {
    auto found = false;

    for (const auto& available_extension : available_extensions) {
      if (std::strcmp(extension, available_extension.extensionName) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      throw std::runtime_error{fmt::format("Required instance extension not available: {}", extension)};
    }
  }

  return required_extensions;
}

} // namespace sbx::graphics
