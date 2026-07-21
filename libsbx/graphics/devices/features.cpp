// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/devices/features.hpp>

#include <cstddef>

namespace sbx::graphics {

// A feature struct is an sType/pNext header followed by VkBool32 members, so
// the helpers walk the bools generically starting behind the header.

template<typename Type>
auto covers(const Type& available, const Type& required, const std::size_t header_size) -> bool {
  const auto count = (sizeof(Type) - header_size) / sizeof(VkBool32);

  const auto* available_bools = reinterpret_cast<const VkBool32*>(reinterpret_cast<const std::byte*>(&available) + header_size);
  const auto* required_bools = reinterpret_cast<const VkBool32*>(reinterpret_cast<const std::byte*>(&required) + header_size);

  for (auto i = std::size_t{0}; i < count; ++i) {
    if (required_bools[i] == true && available_bools[i] != true) {
      return false;
    }
  }

  return true;
}

template<typename Type>
auto merge(Type& out, const Type& required, const Type& optional, const Type& available, const std::size_t header_size) -> void {
  const auto count = (sizeof(Type) - header_size) / sizeof(VkBool32);

  auto* out_bools = reinterpret_cast<VkBool32*>(reinterpret_cast<std::byte*>(&out) + header_size);

  const auto* required_bools = reinterpret_cast<const VkBool32*>(reinterpret_cast<const std::byte*>(&required) + header_size);
  const auto* optional_bools = reinterpret_cast<const VkBool32*>(reinterpret_cast<const std::byte*>(&optional) + header_size);
  const auto* available_bools = reinterpret_cast<const VkBool32*>(reinterpret_cast<const std::byte*>(&available) + header_size);

  for (auto i = std::size_t{0}; i < count; ++i) {
    out_bools[i] = (required_bools[i] == true || (optional_bools[i] == true && available_bools[i] == true)) ? true : false;
  }
}

template<typename Type>
constexpr auto header_size() -> std::size_t {
  return offsetof(Type, pNext) + sizeof(void*);
}

constexpr auto no_header = std::size_t{0};
constexpr auto vulkan11_header = header_size<VkPhysicalDeviceVulkan11Features>();
constexpr auto vulkan12_header = header_size<VkPhysicalDeviceVulkan12Features>();
constexpr auto vulkan13_header = header_size<VkPhysicalDeviceVulkan13Features>();
constexpr auto compute_shader_derivatives_header = header_size<VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR>();

features::features() {
  _compute_shader_derivatives.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR;
  _vulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  _vulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  _vulkan11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  _features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

  _link();
}

features::features(const features& other)
: _features{other._features},
  _vulkan11{other._vulkan11},
  _vulkan12{other._vulkan12},
  _vulkan13{other._vulkan13},
  _compute_shader_derivatives{other._compute_shader_derivatives} {
  _link();
}

auto features::operator=(const features& other) -> features& {
  _features = other._features;
  _vulkan11 = other._vulkan11;
  _vulkan12 = other._vulkan12;
  _vulkan13 = other._vulkan13;
  _compute_shader_derivatives = other._compute_shader_derivatives;

  _link();

  return *this;
}

auto features::required() -> const features& {
  static auto features = graphics::features{};

  // Core
  features.core().samplerAnisotropy = true;
  features.core().multiDrawIndirect = true;
  features.core().fillModeNonSolid = true;
  features.core().independentBlend = true;

  // 1.1
  features.vulkan11().shaderDrawParameters = true;

  // 1.2 — bindless (descriptor indexing), BDA, indirect count, timeline
  features.vulkan12().bufferDeviceAddress = true;
  features.vulkan12().timelineSemaphore = true;
  features.vulkan12().descriptorIndexing = true;
  features.vulkan12().runtimeDescriptorArray = true;
  features.vulkan12().shaderSampledImageArrayNonUniformIndexing = true;
  features.vulkan12().shaderStorageBufferArrayNonUniformIndexing = true;
  features.vulkan12().descriptorBindingSampledImageUpdateAfterBind = true;
  features.vulkan12().descriptorBindingStorageImageUpdateAfterBind = true;
  features.vulkan12().descriptorBindingStorageBufferUpdateAfterBind = true;
  features.vulkan12().descriptorBindingUpdateUnusedWhilePending = true;
  features.vulkan12().descriptorBindingPartiallyBound = true;
  features.vulkan12().descriptorBindingVariableDescriptorCount = true;
  features.vulkan12().drawIndirectCount = true;
  features.vulkan12().scalarBlockLayout = true;
  features.vulkan12().hostQueryReset = true;

  // 1.3 — dynamic rendering, sync2
  features.vulkan13().dynamicRendering = true;
  features.vulkan13().synchronization2 = true;
  features.vulkan13().maintenance4 = true;

  return features;
}

auto features::optional() -> const features& {
  static auto features = graphics::features{};

  // Core
  features.core().sampleRateShading = true;
  features.core().wideLines = true;
  features.core().textureCompressionBC = true;
  features.core().textureCompressionASTC_LDR = true;
  features.core().textureCompressionETC2 = true;
  features.core().vertexPipelineStoresAndAtomics = true;
  features.core().fragmentStoresAndAtomics = true;
  features.core().shaderStorageImageExtendedFormats = true;
  features.core().shaderStorageImageWriteWithoutFormat = true;
  features.core().shaderClipDistance = true;
  features.core().shaderCullDistance = true;
  features.core().geometryShader = true;
  features.core().tessellationShader = true;
  features.core().multiViewport = true;
  features.core().drawIndirectFirstInstance = true;
  features.core().shaderInt16 = true;

  // 1.1
  features.vulkan11().multiview = true;

  // 1.2
  features.vulkan12().shaderInt8 = true;
  features.vulkan12().storagePushConstant8 = true;
  features.vulkan12().storageBuffer8BitAccess = true;
  features.vulkan12().shaderFloat16 = true;

  // 1.3
  features.vulkan13().shaderDemoteToHelperInvocation = true;

  // Extensions
  features.compute_shader_derivatives().computeDerivativeGroupQuads = true;
  features.compute_shader_derivatives().computeDerivativeGroupLinear = true;

  return features;
}

auto features::query(VkPhysicalDevice device) -> features {
  auto features = graphics::features{};

  vkGetPhysicalDeviceFeatures2(device, &features.chain());

  return features;
}

auto features::enabled(const features& required, const features& optional, const features& available) -> features {
  auto features = graphics::features{};

  merge(features.core(), required.core(), optional.core(), available.core(), no_header);
  merge(features.vulkan11(), required.vulkan11(), optional.vulkan11(), available.vulkan11(), vulkan11_header);
  merge(features.vulkan12(), required.vulkan12(), optional.vulkan12(), available.vulkan12(), vulkan12_header);
  merge(features.vulkan13(), required.vulkan13(), optional.vulkan13(), available.vulkan13(), vulkan13_header);
  merge(features.compute_shader_derivatives(), required.compute_shader_derivatives(), optional.compute_shader_derivatives(), available.compute_shader_derivatives(), compute_shader_derivatives_header);

  return features;
}

auto features::supports(const features& required) const -> bool {
  return covers(core(), required.core(), no_header)
    && covers(vulkan11(), required.vulkan11(), vulkan11_header)
    && covers(vulkan12(), required.vulkan12(), vulkan12_header)
    && covers(vulkan13(), required.vulkan13(), vulkan13_header)
    && covers(compute_shader_derivatives(), required.compute_shader_derivatives(), compute_shader_derivatives_header);
}

[[nodiscard]] auto features::chain() noexcept -> VkPhysicalDeviceFeatures2& {
  return _features;
}

[[nodiscard]] auto features::chain() const noexcept -> const VkPhysicalDeviceFeatures2& {
  return _features;
}

[[nodiscard]] auto features::core() noexcept -> VkPhysicalDeviceFeatures& {
  return _features.features;
}

[[nodiscard]] auto features::core() const noexcept -> const VkPhysicalDeviceFeatures& {
  return _features.features;
}

[[nodiscard]] auto features::vulkan11() noexcept -> VkPhysicalDeviceVulkan11Features& {
  return _vulkan11;
}

[[nodiscard]] auto features::vulkan11() const noexcept -> const VkPhysicalDeviceVulkan11Features& {
  return _vulkan11;
}

[[nodiscard]] auto features::vulkan12() noexcept -> VkPhysicalDeviceVulkan12Features& {
  return _vulkan12;
}

[[nodiscard]] auto features::vulkan12() const noexcept -> const VkPhysicalDeviceVulkan12Features& {
  return _vulkan12;
}

[[nodiscard]] auto features::vulkan13() noexcept -> VkPhysicalDeviceVulkan13Features& {
  return _vulkan13;
}

[[nodiscard]] auto features::vulkan13() const noexcept -> const VkPhysicalDeviceVulkan13Features& {
  return _vulkan13;
}

[[nodiscard]] auto features::compute_shader_derivatives() noexcept -> VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR& {
  return _compute_shader_derivatives;
}

[[nodiscard]] auto features::compute_shader_derivatives() const noexcept -> const VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR& {
  return _compute_shader_derivatives;
}

auto features::_link() noexcept -> void {
  _features.pNext = &_vulkan11;
  _vulkan11.pNext = &_vulkan12;
  _vulkan12.pNext = &_vulkan13;
  _vulkan13.pNext = &_compute_shader_derivatives;
  _compute_shader_derivatives.pNext = nullptr;
}

} // namespace sbx::graphics
