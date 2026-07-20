// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_FEATURES_HPP_
#define LIBSBX_GRAPHICS_DEVICES_FEATURES_HPP_

#include <vulkan/vulkan.h>

namespace sbx::graphics {

/**
 * @brief The vulkan feature chain (core + 1.1 + 1.2 + 1.3 + extensions) used to test physical 
 * devices and to enable features at device creation, so the two can never drift apart.
 *
 * The structs link to each other via pNext, so copying relinks the chain.
 */
class features {

public:

  features();

  features(const features& other);

  auto operator=(const features& other) -> features&;

  /**
   * @brief The hard minimum the engine refuses to run without (bindless, dynamic rendering, BDA, timeline semaphores).
   * Device selection filters on this set.
   */
  [[nodiscard]] static auto required() -> const features&;

  /**
   * @brief Features enabled opportunistically when the device supports them (wide lines, geometry/tessellation, 8/16 bit types).
   */
  [[nodiscard]] static auto optional() -> const features&;

  /**
   * @brief Queries the features a physical device actually supports.
   */
  [[nodiscard]] static auto query(VkPhysicalDevice device) -> features;

  /**
   * @brief The set to enable at device creation: required OR (optional AND available).
   */
  [[nodiscard]] static auto enabled(const features& required, const features& optional, const features& available) -> features;

  /**
   * @brief Checks that every feature enabled in `required` is also enabled here.
   */
  [[nodiscard]] auto supports(const features& required) const -> bool;

  [[nodiscard]] auto chain() noexcept -> VkPhysicalDeviceFeatures2&;

  [[nodiscard]] auto chain() const noexcept -> const VkPhysicalDeviceFeatures2&;

  [[nodiscard]] auto core() noexcept -> VkPhysicalDeviceFeatures&;

  [[nodiscard]] auto core() const noexcept -> const VkPhysicalDeviceFeatures&;

  [[nodiscard]] auto vulkan11() noexcept -> VkPhysicalDeviceVulkan11Features&;

  [[nodiscard]] auto vulkan11() const noexcept -> const VkPhysicalDeviceVulkan11Features&;

  [[nodiscard]] auto vulkan12() noexcept -> VkPhysicalDeviceVulkan12Features&;

  [[nodiscard]] auto vulkan12() const noexcept -> const VkPhysicalDeviceVulkan12Features&;

  [[nodiscard]] auto vulkan13() noexcept -> VkPhysicalDeviceVulkan13Features&;

  [[nodiscard]] auto vulkan13() const noexcept -> const VkPhysicalDeviceVulkan13Features&;

  [[nodiscard]] auto compute_shader_derivatives() noexcept -> VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR&;

  [[nodiscard]] auto compute_shader_derivatives() const noexcept -> const VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR&;

private:

  auto _link() noexcept -> void;

  VkPhysicalDeviceFeatures2 _features{};
  VkPhysicalDeviceVulkan11Features _vulkan11{};
  VkPhysicalDeviceVulkan12Features _vulkan12{};
  VkPhysicalDeviceVulkan13Features _vulkan13{};
  VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR _compute_shader_derivatives{};

}; // class features

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_FEATURES_HPP_
