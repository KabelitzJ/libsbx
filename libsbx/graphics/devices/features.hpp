// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_DEVICES_FEATURES_HPP_
#define LIBSBX_GRAPHICS_DEVICES_FEATURES_HPP_

#include <libsbx/graphics/vulkan.hpp>

namespace sbx::graphics {

/**
 * @brief The vulkan feature chain (core + 1.1 + 1.2 + 1.3 + extensions) used
 * to test physical devices and to enable features at device creation, so the
 * two can never drift apart.
 *
 * The structs link to each other via pNext, so copying relinks the chain.
 */
class device_features {

public:

  device_features() {
    _compute_shader_derivatives.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR;
    _vulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    _vulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    _vulkan11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    _features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    _link();
  }

  device_features(const device_features& other)
  : _features{other._features},
    _vulkan11{other._vulkan11},
    _vulkan12{other._vulkan12},
    _vulkan13{other._vulkan13},
    _compute_shader_derivatives{other._compute_shader_derivatives} {
    _link();
  }

  auto operator=(const device_features& other) -> device_features& {
    _features = other._features;
    _vulkan11 = other._vulkan11;
    _vulkan12 = other._vulkan12;
    _vulkan13 = other._vulkan13;
    _compute_shader_derivatives = other._compute_shader_derivatives;

    _link();

    return *this;
  }

  /**
   * @brief The hard minimum the engine refuses to run without (bindless,
   * dynamic rendering, BDA, timeline semaphores, ...). Device selection
   * filters on this set.
   */
  [[nodiscard]] static auto required() -> device_features;

  /**
   * @brief Features enabled opportunistically when the device supports them
   * (wide lines, geometry/tessellation, 8/16 bit types, ...).
   */
  [[nodiscard]] static auto optional() -> device_features;

  /**
   * @brief Queries the features a physical device actually supports.
   */
  [[nodiscard]] static auto query(VkPhysicalDevice device) -> device_features;

  /**
   * @brief The set to enable at device creation:
   * required ∪ (optional ∩ available).
   */
  [[nodiscard]] static auto enabled(const device_features& required, const device_features& optional, const device_features& available) -> device_features;

  /**
   * @brief Checks that every feature enabled in `required` is also enabled here.
   */
  [[nodiscard]] auto supports(const device_features& required) const -> bool;

  [[nodiscard]] auto chain() noexcept -> VkPhysicalDeviceFeatures2& {
    return _features;
  }

  [[nodiscard]] auto chain() const noexcept -> const VkPhysicalDeviceFeatures2& {
    return _features;
  }

  [[nodiscard]] auto core() noexcept -> VkPhysicalDeviceFeatures& {
    return _features.features;
  }

  [[nodiscard]] auto core() const noexcept -> const VkPhysicalDeviceFeatures& {
    return _features.features;
  }

  [[nodiscard]] auto vulkan11() noexcept -> VkPhysicalDeviceVulkan11Features& {
    return _vulkan11;
  }

  [[nodiscard]] auto vulkan11() const noexcept -> const VkPhysicalDeviceVulkan11Features& {
    return _vulkan11;
  }

  [[nodiscard]] auto vulkan12() noexcept -> VkPhysicalDeviceVulkan12Features& {
    return _vulkan12;
  }

  [[nodiscard]] auto vulkan12() const noexcept -> const VkPhysicalDeviceVulkan12Features& {
    return _vulkan12;
  }

  [[nodiscard]] auto vulkan13() noexcept -> VkPhysicalDeviceVulkan13Features& {
    return _vulkan13;
  }

  [[nodiscard]] auto vulkan13() const noexcept -> const VkPhysicalDeviceVulkan13Features& {
    return _vulkan13;
  }

  [[nodiscard]] auto compute_shader_derivatives() noexcept -> VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR& {
    return _compute_shader_derivatives;
  }

  [[nodiscard]] auto compute_shader_derivatives() const noexcept -> const VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR& {
    return _compute_shader_derivatives;
  }

private:

  auto _link() noexcept -> void {
    _features.pNext = &_vulkan11;
    _vulkan11.pNext = &_vulkan12;
    _vulkan12.pNext = &_vulkan13;
    _vulkan13.pNext = &_compute_shader_derivatives;
    _compute_shader_derivatives.pNext = nullptr;
  }

  VkPhysicalDeviceFeatures2 _features{};
  VkPhysicalDeviceVulkan11Features _vulkan11{};
  VkPhysicalDeviceVulkan12Features _vulkan12{};
  VkPhysicalDeviceVulkan13Features _vulkan13{};
  VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR _compute_shader_derivatives{};

}; // class device_features

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_DEVICES_FEATURES_HPP_
