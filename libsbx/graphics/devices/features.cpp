// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/features.hpp>

#include <cstddef>

namespace sbx::graphics {

namespace {

// A feature struct is an sType/pNext header followed by VkBool32 members, so
// the helpers walk the bools generically starting behind the header.

template<typename Type>
auto covers(const Type& available, const Type& required, const std::size_t header_size) -> bool {
  const auto count = (sizeof(Type) - header_size) / sizeof(VkBool32);

  const auto* available_bools = reinterpret_cast<const VkBool32*>(reinterpret_cast<const std::byte*>(&available) + header_size);
  const auto* required_bools = reinterpret_cast<const VkBool32*>(reinterpret_cast<const std::byte*>(&required) + header_size);

  for (auto i = std::size_t{0}; i < count; ++i) {
    if (required_bools[i] == VK_TRUE && available_bools[i] != VK_TRUE) {
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
    out_bools[i] = (required_bools[i] == VK_TRUE || (optional_bools[i] == VK_TRUE && available_bools[i] == VK_TRUE)) ? VK_TRUE : VK_FALSE;
  }
}

constexpr auto no_header = std::size_t{0};
constexpr auto vulkan11_header = offsetof(VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess);
constexpr auto vulkan12_header = offsetof(VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge);
constexpr auto vulkan13_header = offsetof(VkPhysicalDeviceVulkan13Features, robustImageAccess);
constexpr auto compute_shader_derivatives_header = offsetof(VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR, computeDerivativeGroupQuads);

} // namespace

auto device_features::required() -> device_features {
  auto features = device_features{};

  // Core
  features.core().samplerAnisotropy = VK_TRUE;
  features.core().multiDrawIndirect = VK_TRUE;
  features.core().fillModeNonSolid = VK_TRUE;
  features.core().independentBlend = VK_TRUE;

  // 1.1
  features.vulkan11().shaderDrawParameters = VK_TRUE;

  // 1.2 — bindless (descriptor indexing), BDA, indirect count, timeline
  features.vulkan12().bufferDeviceAddress = VK_TRUE;
  features.vulkan12().timelineSemaphore = VK_TRUE;
  features.vulkan12().descriptorIndexing = VK_TRUE;
  features.vulkan12().runtimeDescriptorArray = VK_TRUE;
  features.vulkan12().shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
  features.vulkan12().shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
  features.vulkan12().descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
  features.vulkan12().descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
  features.vulkan12().descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
  features.vulkan12().descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
  features.vulkan12().descriptorBindingPartiallyBound = VK_TRUE;
  features.vulkan12().descriptorBindingVariableDescriptorCount = VK_TRUE;
  features.vulkan12().drawIndirectCount = VK_TRUE;
  features.vulkan12().scalarBlockLayout = VK_TRUE;
  features.vulkan12().hostQueryReset = VK_TRUE;

  // 1.3 — dynamic rendering, sync2
  features.vulkan13().dynamicRendering = VK_TRUE;
  features.vulkan13().synchronization2 = VK_TRUE;
  features.vulkan13().maintenance4 = VK_TRUE;

  return features;
}

auto device_features::optional() -> device_features {
  auto features = device_features{};

  // Core — mirrors the enable-if-available set of the previous engine. The
  // texture compression entries were a BC → ASTC → ETC2 fallback chain there;
  // enabling every supported one is equivalent (the asset pipeline picks).
  features.core().sampleRateShading = VK_TRUE;
  features.core().wideLines = VK_TRUE;
  features.core().textureCompressionBC = VK_TRUE;
  features.core().textureCompressionASTC_LDR = VK_TRUE;
  features.core().textureCompressionETC2 = VK_TRUE;
  features.core().vertexPipelineStoresAndAtomics = VK_TRUE;
  features.core().fragmentStoresAndAtomics = VK_TRUE;
  features.core().shaderStorageImageExtendedFormats = VK_TRUE;
  features.core().shaderStorageImageWriteWithoutFormat = VK_TRUE;
  features.core().shaderClipDistance = VK_TRUE;
  features.core().shaderCullDistance = VK_TRUE;
  features.core().geometryShader = VK_TRUE;
  features.core().tessellationShader = VK_TRUE;
  features.core().multiViewport = VK_TRUE;
  features.core().drawIndirectFirstInstance = VK_TRUE;
  features.core().shaderInt16 = VK_TRUE;

  // 1.1
  features.vulkan11().multiview = VK_TRUE;

  // 1.2 — small types
  features.vulkan12().shaderInt8 = VK_TRUE;
  features.vulkan12().storagePushConstant8 = VK_TRUE;
  features.vulkan12().storageBuffer8BitAccess = VK_TRUE;
  features.vulkan12().shaderFloat16 = VK_TRUE;

  // 1.3
  features.vulkan13().shaderDemoteToHelperInvocation = VK_TRUE;

  // Extensions
  features.compute_shader_derivatives().computeDerivativeGroupQuads = VK_TRUE;
  features.compute_shader_derivatives().computeDerivativeGroupLinear = VK_TRUE;

  return features;
}

auto device_features::query(VkPhysicalDevice device) -> device_features {
  auto features = device_features{};

  vkGetPhysicalDeviceFeatures2(device, &features.chain());

  return features;
}

auto device_features::enabled(const device_features& required, const device_features& optional, const device_features& available) -> device_features {
  auto features = device_features{};

  merge(features.core(), required.core(), optional.core(), available.core(), no_header);
  merge(features.vulkan11(), required.vulkan11(), optional.vulkan11(), available.vulkan11(), vulkan11_header);
  merge(features.vulkan12(), required.vulkan12(), optional.vulkan12(), available.vulkan12(), vulkan12_header);
  merge(features.vulkan13(), required.vulkan13(), optional.vulkan13(), available.vulkan13(), vulkan13_header);
  merge(features.compute_shader_derivatives(), required.compute_shader_derivatives(), optional.compute_shader_derivatives(), available.compute_shader_derivatives(), compute_shader_derivatives_header);

  return features;
}

auto device_features::supports(const device_features& required) const -> bool {
  return covers(core(), required.core(), no_header)
    && covers(vulkan11(), required.vulkan11(), vulkan11_header)
    && covers(vulkan12(), required.vulkan12(), vulkan12_header)
    && covers(vulkan13(), required.vulkan13(), vulkan13_header)
    && covers(compute_shader_derivatives(), required.compute_shader_derivatives(), compute_shader_derivatives_header);
}

} // namespace sbx::graphics
