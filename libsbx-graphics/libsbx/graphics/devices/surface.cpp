// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/surface.hpp>

// #include <vulkan/vk_enum_string_helper.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/devices/devices_module.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

surface::surface(const instance& instance, const physical_device& physical_device, const logical_device& logical_device) {
  auto& devices_module = core::engine::get_module<devices::devices_module>();

  auto& window = devices_module.window();

  validate(glfwCreateWindowSurface(instance, window, nullptr, &_handle));

  validate(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, _handle, &_capabilities));

  auto surface_format_count = std::uint32_t{0};
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, _handle, &surface_format_count, nullptr);
  
	auto surface_formats = utility::make_vector<VkSurfaceFormatKHR>(surface_format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, _handle, &surface_format_count, surface_formats.data());

  if (surface_formats.empty()) {
    throw std::runtime_error("No surface formats available");
  }

  _format = _choose_swap_surface_format(surface_formats);

  const auto& present_queue = logical_device.queue<queue::type::present>();

	auto present_support = std::uint32_t{0};
	vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, present_queue.family(), _handle, &present_support);

	if (!present_support) {
		throw std::runtime_error("Graphics queue family does not have presentation support");
  }
}

surface::~surface() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& instance = graphics_module.instance();
  vkDestroySurfaceKHR(instance, _handle, nullptr);
}

auto surface::handle() const noexcept -> const VkSurfaceKHR& {
  return _handle;
}

surface::operator const VkSurfaceKHR&() const noexcept {
  return _handle;
}

auto surface::capabilities() const noexcept -> VkSurfaceCapabilitiesKHR {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& physical_device = graphics_module.physical_device();

  auto capabilities = VkSurfaceCapabilitiesKHR{};

  validate(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, _handle, &capabilities));

  return capabilities;
}

auto surface::format() const noexcept -> const VkSurfaceFormatKHR& {
  return _format;
}

auto surface::current_extent() const noexcept -> VkExtent2D {
  return capabilities().currentExtent;
}

auto surface::_choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) -> VkSurfaceFormatKHR {
  for (const auto& available_format : available_formats) {
    if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB && available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return available_format;
    }
  }

  utility::logger<"graphics">::warn("Could not find desired surface format");

  return available_formats[0];
}

} // namespace sbx::graphics
