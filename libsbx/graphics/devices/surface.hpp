// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_SURFACE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_SURFACE_HPP_

#include <vulkan/vulkan.hpp>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>

namespace sbx::graphics {

class surface : public utility::noncopyable {

public:

  surface(const instance& instance, const physical_device& physical_device, const logical_device& logical_device);

  ~surface();

  auto handle() const noexcept -> const VkSurfaceKHR&;

  operator const VkSurfaceKHR&() const noexcept;

  auto capabilities() const noexcept -> VkSurfaceCapabilitiesKHR;

  auto format() const noexcept -> const VkSurfaceFormatKHR&;

  auto current_extent() const noexcept -> VkExtent2D;

private:

  auto _choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) -> VkSurfaceFormatKHR;

  VkSurfaceKHR _handle{};
	VkSurfaceCapabilitiesKHR _capabilities{};
	VkSurfaceFormatKHR _format{};

}; // class surface

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_SURFACE_HPP_
