// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_PHYSICAL_DEVICE_HPP_
#define LIBSBX_GRAPHICS_DEVICES_PHYSICAL_DEVICE_HPP_

#include <vector>
#include <cstring>
#include <cinttypes>

#include <vulkan/vulkan.hpp>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/graphics/devices/instance.hpp>

namespace sbx::graphics {

class physical_device : public utility::noncopyable {

public:

  struct device_features {
    VkPhysicalDeviceFeatures2 core{};
    VkPhysicalDeviceVulkan11Features vulkan11{};
    VkPhysicalDeviceVulkan12Features vulkan12{};
    VkPhysicalDeviceVulkan13Features vulkan13{};
    VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR compute_shader_derivatives{};

    device_features() {
      compute_shader_derivatives.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR;
      compute_shader_derivatives.pNext = nullptr;

      vulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
      vulkan13.pNext = &compute_shader_derivatives;

      vulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
      vulkan12.pNext = &vulkan13;

      vulkan11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
      vulkan11.pNext = &vulkan12;

      core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
      core.pNext = &vulkan11;
    }

  }; // struct features

  physical_device(const instance& instance);

  ~physical_device();

  auto handle() const noexcept -> const VkPhysicalDevice&;

  operator const VkPhysicalDevice&() const noexcept;

	auto properties() const -> const VkPhysicalDeviceProperties&;

	auto features() const -> const device_features&;

	auto memory_properties() const -> const VkPhysicalDeviceMemoryProperties&;

	auto msaa_samples() const -> const VkSampleCountFlagBits&;

  auto find_memory_type(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const -> std::uint32_t;

  auto find_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const -> VkFormat;

private:

  auto _choose_device(const std::vector<VkPhysicalDevice>& devices) -> VkPhysicalDevice;

  auto _score_device(const VkPhysicalDevice& device) -> std::uint32_t;

  auto _get_max_usable_sample_count() const -> VkSampleCountFlagBits;

  VkPhysicalDevice _handle{};
  VkPhysicalDeviceProperties _properties{};
	// VkPhysicalDeviceFeatures2 _features{};
  device_features _features;
	VkPhysicalDeviceMemoryProperties _memory_properties{};
  VkSampleCountFlagBits _msaa_samples{};

}; // class physical_device

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_PHYSICAL_DEVICE_HPP_

