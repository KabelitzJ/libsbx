// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/logical_device.hpp>

#include <libsbx/utility/target.hpp>
#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/devices/validation_layers.hpp>
#include <libsbx/graphics/devices/extensions.hpp>

namespace sbx::graphics {

// [NOTE] KAJ 2023-02-23 : Maybe rework the queue creation

static auto _print_queue_families(const VkQueueFamilyProperties& queue_family_properties) -> std::string {
  auto result = std::string{};

  if (queue_family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
    if (!result.empty()) {
      result += "|";
    }
    result += "Graphics";
  }

  if (queue_family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
    if (!result.empty()) {
      result += "|";
    }
    result += "Compute";
  }

  if (queue_family_properties.queueFlags & VK_QUEUE_TRANSFER_BIT) {
    if (!result.empty()) {
      result += "|";
    }
    result += "Transfer";
  }

  if (queue_family_properties.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) {
    if (!result.empty()) {
      result += "|";
    }
    result += "Sparse Binding";
  }

  return result;
};

auto queue::handle() const noexcept -> const VkQueue& {
  return _handle;
}

queue::operator const VkQueue&() const noexcept {
  return _handle;
}

auto queue::family() const noexcept -> std::uint32_t {
  return _family;
}

auto queue::wait_idle() const -> void {
  validate(vkQueueWaitIdle(_handle));
}

logical_device::logical_device(const physical_device& physical_device) {
  _create_logical_device(physical_device);
}

logical_device::~logical_device() {
  wait_idle();

  vkDestroyDevice(_handle, nullptr);
}

auto logical_device::handle() const noexcept -> const VkDevice& {
  return _handle;
}

logical_device::operator const VkDevice&() const noexcept {
  return _handle;
}

auto logical_device::enabled_features() const -> const physical_device::device_features& {
  return _enabled_features;
}

auto logical_device::wait_idle() const -> void {
  validate(vkDeviceWaitIdle(_handle));
}

auto logical_device::_get_queue_family_indices(const physical_device& physical_device) const -> queue_family_indices {
  auto result = queue_family_indices{};

  auto device_queue_family_property_count = std::uint32_t{0};
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &device_queue_family_property_count, nullptr);

	auto device_queue_family_properties = utility::make_vector<VkQueueFamilyProperties>(device_queue_family_property_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &device_queue_family_property_count, device_queue_family_properties.data());

  for (auto i = std::uint32_t{0}; i < device_queue_family_property_count; ++i) {
    utility::logger<"graphics">::debug("Queue Family {} supports {} queues of type [{}]", i, device_queue_family_properties[i].queueCount, _print_queue_families(device_queue_family_properties[i]));

    // [NOTE] KAJ 2023-03-20 : Always pick the queue that is the most specialized for the task i.e. has the least flags other than the one we are looking for
		if (device_queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			if (!result.graphics) {
        result.graphics = i;
      } else {
        const auto old_queue = device_queue_family_properties[*result.graphics];

        if (std::popcount(device_queue_family_properties[i].queueFlags) < std::popcount(old_queue.queueFlags)) {
          result.graphics = i;
        }
      }

      if (device_queue_family_properties[i].queueCount > 0u) {
        if (!result.present) {
          result.present = i;
        } else {
          const auto old_queue = device_queue_family_properties[*result.present];

          if (std::popcount(device_queue_family_properties[i].queueFlags) < std::popcount(old_queue.queueFlags)) {
            result.present = i;
          } 
        }
      }
		}

		if (device_queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			if (!result.compute) {
        result.compute = i;
      } else {
        const auto old_queue = device_queue_family_properties[*result.compute];

        if (std::popcount(device_queue_family_properties[i].queueFlags) < std::popcount(old_queue.queueFlags)) {
          result.compute = i;
        }
      }
		}

		if (device_queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
			if (!result.transfer) {
        result.transfer = i;
      } else {
        const auto old_queue = device_queue_family_properties[*result.transfer];

        if (std::popcount(device_queue_family_properties[i].queueFlags) < std::popcount(old_queue.queueFlags)) {
          result.transfer = i;
        }
      }
		}
	}

	if (!result.graphics) {
		throw std::runtime_error("Failed to find suitable graphics queue family");
  }

  utility::logger<"graphics">::debug("Selected graphics queue family: {}", *result.graphics);

  if (!result.present) {
    result.present = result.graphics;
  }

  utility::logger<"graphics">::debug("Selected present queue family: {}", *result.present);

  if (!result.compute) {
    throw std::runtime_error("Failed to find suitable compute queue family");
  }

  utility::logger<"graphics">::debug("Selected compute queue family: {}", *result.compute);

  if (!result.transfer) {
    throw std::runtime_error("Failed to find suitable transfer queue family");
  }

  utility::logger<"graphics">::debug("Selected transfer queue family: {}", *result.transfer);

  return result;
}

auto logical_device::_get_enabled_features(const physical_device& physical_device) const -> physical_device::device_features {
  const auto& physical_device_features = physical_device.features();
  
  auto& available_core_features = physical_device_features.core.features;
  auto& available_vulkan11_features = physical_device_features.vulkan11;
  auto& available_vulkan12_features = physical_device_features.vulkan12;
  auto& available_vulkan13_features = physical_device_features.vulkan13;
  auto& available_compute_shader_derivatives_features = physical_device_features.compute_shader_derivatives;
  // auto& available_device_address_features = physical_device_features.device_address;
  // auto& available_descriptor_indexing_features = physical_device_features.descriptor_indexing;

	auto enabled_features = physical_device::device_features{};

  auto& enabled_core_features = enabled_features.core.features;
  auto& enabled_vulkan11_features = enabled_features.vulkan11;
  auto& enabled_vulkan12_features = enabled_features.vulkan12;
  auto& enabled_vulkan13_features = enabled_features.vulkan13;
  auto& enabled_compute_shader_derivatives_features = enabled_features.compute_shader_derivatives;
  // auto& enabled_device_address_features = enabled_features.device_address; 
  // auto& enabled_descriptor_indexing_features = enabled_features.descriptor_indexing; 

	if (available_core_features.sampleRateShading) {
		enabled_core_features.sampleRateShading = true;
  }

	if (available_core_features.fillModeNonSolid) {
		enabled_core_features.fillModeNonSolid = true;

		if (available_core_features.wideLines) {
		  enabled_core_features.wideLines = true;
    }
	} else {
		utility::logger<"graphics">::warn("Selected GPU does not support wireframe pipelines");
	}

	if (available_core_features.samplerAnisotropy) {
		enabled_core_features.samplerAnisotropy = true;
  } else {
		utility::logger<"graphics">::warn("Selected GPU does not support sampler anisotropy");
  }

	if (available_core_features.textureCompressionBC) {
		enabled_core_features.textureCompressionBC = true;
    utility::logger<"graphics">::debug("Selected GPU supports BC texture compression");
  } else if (available_core_features.textureCompressionASTC_LDR) {
		enabled_core_features.textureCompressionASTC_LDR = true;
    utility::logger<"graphics">::debug("Selected GPU supports ASTC_LDR texture compression");
  } else if (available_core_features.textureCompressionETC2) {
		enabled_core_features.textureCompressionETC2 = true;
    utility::logger<"graphics">::debug("Selected GPU supports ETC2 texture compression");
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support texture compression");
  }

	if (available_core_features.vertexPipelineStoresAndAtomics) {
		enabled_core_features.vertexPipelineStoresAndAtomics = true;
  } else {
		utility::logger<"graphics">::warn("Selected GPU does not support vertex pipeline stores and atomics");
  }

	if (available_core_features.fragmentStoresAndAtomics) {
		enabled_core_features.fragmentStoresAndAtomics = true;
  } else {
		utility::logger<"graphics">::warn("Selected GPU does not support fragment stores and atomics");
  }

	if (available_core_features.shaderStorageImageExtendedFormats) {
		enabled_core_features.shaderStorageImageExtendedFormats = true;
  } else {
		utility::logger<"graphics">::warn("Selected GPU does not support shader storage extended formats");
  }

	if (available_core_features.shaderStorageImageWriteWithoutFormat) {
		enabled_core_features.shaderStorageImageWriteWithoutFormat = true;
  } else {
		utility::logger<"graphics">::warn("Selected GPU does not support shader storage write without format");
  }

  if (available_core_features.shaderClipDistance) {
		enabled_core_features.shaderClipDistance = true;
  } else {
		utility::logger<"graphics">::warn("Selected GPU does not support shader clip distance");
  }

  if (available_core_features.shaderCullDistance) {
		enabled_core_features.shaderCullDistance = true;
  } else {
		utility::logger<"graphics">::warn("Selected GPU does not support shader cull distance");
  }

	if (available_core_features.geometryShader) {
		enabled_core_features.geometryShader = true;
  } else {
		utility::logger<"graphics">::warn("Selected GPU does not support geometry shaders");
  }

	if (available_core_features.tessellationShader) {
		enabled_core_features.tessellationShader = true;
  } else {
		utility::logger<"graphics">::warn("Selected GPU does not support tessellation shaders");
  }

	if (available_core_features.multiViewport) {
		enabled_core_features.multiViewport = true;
  } else {
		utility::logger<"graphics">::warn("Selected GPU does not support multi viewports");
  }

  if (available_core_features.independentBlend) {
    enabled_core_features.independentBlend = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support independent  blend");
  }

  if (available_core_features.multiDrawIndirect) {
    enabled_core_features.multiDrawIndirect = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support multi draw indirect");
  }

  if (available_core_features.drawIndirectFirstInstance) {
    enabled_core_features.drawIndirectFirstInstance = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support draw indirect first instance");
  }

  if (available_core_features.shaderInt16) {
    enabled_core_features.shaderInt16 = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support shader int16");
  }

  if (available_vulkan11_features.shaderDrawParameters) {
    enabled_vulkan11_features.shaderDrawParameters = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support shader draw parameters");
  }

  if (available_vulkan12_features.bufferDeviceAddress) {
    enabled_vulkan12_features.bufferDeviceAddress = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support buffer device address");
  }

  if (available_vulkan12_features.shaderSampledImageArrayNonUniformIndexing) {
    enabled_vulkan12_features.shaderSampledImageArrayNonUniformIndexing = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support sampled image array non uniform indexing");
  }

  if (available_vulkan12_features.runtimeDescriptorArray) {
    enabled_vulkan12_features.runtimeDescriptorArray = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support runtime descriptor array");
  }

  if (available_vulkan12_features.descriptorBindingVariableDescriptorCount) {
    enabled_vulkan12_features.descriptorBindingVariableDescriptorCount = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support descriptor binding variable descriptor count");
  }

  if (available_vulkan12_features.descriptorBindingPartiallyBound) {
    enabled_vulkan12_features.descriptorBindingPartiallyBound = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support descriptor binding partially bound");
  }

  if (available_vulkan12_features.shaderFloat16) {
    enabled_vulkan12_features.shaderFloat16 = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support shader float16");
  }

  if (available_vulkan13_features.synchronization2) {
    enabled_vulkan13_features.synchronization2 = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support synchronization 2");
  }

  if (available_vulkan13_features.dynamicRendering) {
    enabled_vulkan13_features.dynamicRendering = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support dynamice rendering 2");
  }

  if (available_vulkan13_features.shaderDemoteToHelperInvocation) {
    enabled_vulkan13_features.shaderDemoteToHelperInvocation = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support demote to helper invocation");
  }

  if (available_compute_shader_derivatives_features.computeDerivativeGroupQuads) {
    enabled_compute_shader_derivatives_features.computeDerivativeGroupQuads = true;
  } else {
    utility::logger<"graphics">::warn("Selected GPU does not support compute shader derivatives");
  }

  return enabled_features;
}

auto logical_device::_create_logical_device(const physical_device& physical_device) -> void {
  const auto queue_family_indices = _get_queue_family_indices(physical_device);

  const auto graphics_queue_family_index = queue_family_indices.graphics.value();
  const auto present_queue_family_index = queue_family_indices.present.value();
  const auto compute_queue_family_index = queue_family_indices.compute.value();
  const auto transfer_queue_family_index = queue_family_indices.transfer.value();

  auto queue_create_infos = std::vector<VkDeviceQueueCreateInfo>{};
	auto queue_priorities = 0.0f;

  auto graphics_queue_create_info = VkDeviceQueueCreateInfo{};
  graphics_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  graphics_queue_create_info.queueFamilyIndex = graphics_queue_family_index;
  graphics_queue_create_info.queueCount = (present_queue_family_index != graphics_queue_family_index) ? 2u : 1u;
  graphics_queue_create_info.pQueuePriorities = &queue_priorities;

  queue_create_infos.emplace_back(graphics_queue_create_info);

  if (compute_queue_family_index != graphics_queue_family_index) {
    auto compute_queue_create_info = VkDeviceQueueCreateInfo{};
    compute_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    compute_queue_create_info.queueFamilyIndex = compute_queue_family_index;
    compute_queue_create_info.queueCount = 1;
    compute_queue_create_info.pQueuePriorities = &queue_priorities;

    queue_create_infos.emplace_back(compute_queue_create_info);
  }

  if (transfer_queue_family_index != graphics_queue_family_index && transfer_queue_family_index != compute_queue_family_index) {
    auto transfer_queue_create_info = VkDeviceQueueCreateInfo{};
    transfer_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    transfer_queue_create_info.queueFamilyIndex = transfer_queue_family_index;
    transfer_queue_create_info.queueCount = 1;
    transfer_queue_create_info.pQueuePriorities = &queue_priorities;

    queue_create_infos.emplace_back(transfer_queue_create_info);
  }

  _enabled_features = _get_enabled_features(physical_device);

  const auto instance_validation_layers = validation_layers::instance();
  const auto device_extensions = extensions::device();

	auto device_create_info = VkDeviceCreateInfo{};
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = &_enabled_features.core;
	device_create_info.queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size());
	device_create_info.pQueueCreateInfos = queue_create_infos.data();
  device_create_info.enabledLayerCount = static_cast<std::uint32_t>(instance_validation_layers.size());
  device_create_info.ppEnabledLayerNames = instance_validation_layers.data();
	device_create_info.enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size());
	device_create_info.ppEnabledExtensionNames = device_extensions.data();
	device_create_info.pEnabledFeatures = nullptr;

	validate(vkCreateDevice(physical_device, &device_create_info, nullptr, &_handle));

  _get_queue<queue::type::graphics>(graphics_queue_family_index);
  _get_queue<queue::type::present>(present_queue_family_index);
  _get_queue<queue::type::compute>(compute_queue_family_index);
  _get_queue<queue::type::transfer>(transfer_queue_family_index);

  utility::logger<"graphics">::debug("Created logical device with {} unique queues", queue_create_infos.size());
}

} // namespace sbx::graphics
