// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/surface.hpp>

#include <GLFW/glfw3.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/iterator.hpp>
#include <libsbx/utility/overload.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/platform/platform_module.hpp>
#include <libsbx/platform/window.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

surface::surface(const instance& instance, const physical_device& physical_device, const logical_device& logical_device) {
  auto& platform_module = core::engine::get_module<platform::platform_module>();

  auto& window = platform_module.window();

  validate(glfwCreateWindowSurface(instance, window, nullptr, &_handle), "glfwCreateWindowSurface");

  const auto& graphics_queue = logical_device.graphics_queue();

	auto present_support = std::uint32_t{0};
	validate(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, graphics_queue.family, _handle, &present_support), "vkGetPhysicalDeviceSurfaceSupportKHR");

	if (!present_support) {
		throw std::runtime_error("Graphics queue family does not have presentation support");
  }

  validate(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, _handle, &_capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

  auto surface_format_count = std::uint32_t{0};
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, _handle, &surface_format_count, nullptr);
  
	auto surface_formats = utility::make_vector<VkSurfaceFormatKHR>(surface_format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, _handle, &surface_format_count, surface_formats.data());

  if (surface_formats.empty()) {
    throw std::runtime_error("No surface formats available");
  }

  _format = _choose_swap_surface_format(surface_formats);
}

surface::~surface() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& instance = graphics_module.instance();

  vkDestroySurfaceKHR(instance, _handle, nullptr);
}

auto surface::handle() const noexcept -> handle_type {
  return _handle;
}

surface::operator handle_type() const noexcept {
  return _handle;
}

auto surface::capabilities() const noexcept -> VkSurfaceCapabilitiesKHR {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& physical_device = graphics_module.physical_device();

  auto capabilities = VkSurfaceCapabilitiesKHR{};

  validate(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, _handle, &capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

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
