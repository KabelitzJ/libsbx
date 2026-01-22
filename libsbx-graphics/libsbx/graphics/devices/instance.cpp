// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/instance.hpp>

#include <libsbx/utility/target.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/devices/extensions.hpp>
#include <libsbx/graphics/devices/validation_layers.hpp>
#include <libsbx/graphics/devices/debug_messenger.hpp>

namespace sbx::graphics {

instance::instance()
  : _handle{VK_NULL_HANDLE} {
  auto api_version = VK_API_VERSION_1_0;
  vkEnumerateInstanceVersion(&api_version);
  utility::logger<"graphics">::info("Latest available Vulkan API verion: {}.{}.{}", VK_VERSION_MAJOR(api_version), VK_VERSION_MINOR(api_version), VK_VERSION_PATCH(api_version));

  api_version = VK_API_VERSION_1_4;

  utility::logger<"graphics">::info("Used Vulkan API verion: {}.{}.{}", VK_VERSION_MAJOR(api_version), VK_VERSION_MINOR(api_version), VK_VERSION_PATCH(api_version));

  auto app_info = VkApplicationInfo{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Demo";
  app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.pEngineName = "Sandbox";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = api_version;

  const auto instance_extensions = extensions::instance();
  const auto instance_layers = validation_layers::instance();

  auto instance_create_info = VkInstanceCreateInfo{};
  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create_info.pNext = debug_messenger::create_info();
  instance_create_info.pApplicationInfo = &app_info;
  instance_create_info.enabledLayerCount = static_cast<std::uint32_t>(instance_layers.size());
  instance_create_info.ppEnabledLayerNames = instance_layers.data();
  instance_create_info.enabledExtensionCount = static_cast<std::uint32_t>(instance_extensions.size());
  instance_create_info.ppEnabledExtensionNames = instance_extensions.data();

  validate(vkCreateInstance(&instance_create_info, nullptr, &_handle));

  if constexpr (utility::build_configuration_v == utility::build_configuration::debug) {
    validate(debug_messenger::create(*this));
  }
}

instance::~instance() {
  if constexpr (utility::build_configuration_v == utility::build_configuration::debug) {
    debug_messenger::destroy(*this);
  }

  vkDestroyInstance(_handle, nullptr);
}

auto instance::handle() const noexcept -> const VkInstance& {
  return _handle;
}

instance::operator const VkInstance&() const noexcept {
  return _handle;
}

} // namespace sbx::graphics
