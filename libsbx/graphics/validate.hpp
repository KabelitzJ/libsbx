// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_VULKAN_HPP_
#define LIBSBX_GRAPHICS_VULKAN_HPP_

#include <string_view>

#include <vulkan/vulkan.h>

#include <fmt/format.h>

namespace sbx::graphics {

/**
 * @brief Throws if a vulkan call did not return VK_SUCCESS.
 */
auto validate(const VkResult result, const std::string_view what) -> void;

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_VULKAN_HPP_
