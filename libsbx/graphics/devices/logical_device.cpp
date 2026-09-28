// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/devices/logical_device.hpp>

#include <algorithm>
#include <optional>
#include <unordered_set>
#include <vector>

#include <libsbx/core/engine.hpp>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/graphics/validate.hpp>

#include <libsbx/graphics/devices/features.hpp>
#include <libsbx/graphics/devices/layers.hpp>
#include <libsbx/graphics/devices/extensions.hpp>

namespace sbx::graphics {

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

auto queue::handle() const noexcept -> handle_type {
  return _handle;
}

queue::operator handle_type() const noexcept {
  return _handle;
}

auto queue::family() const noexcept -> std::uint32_t {
  return _family;
}

auto queue::lock() const -> std::unique_lock<std::mutex> {
  return std::unique_lock{*_mutex};
}

struct queue_family_indices {
  std::optional<std::uint32_t> graphics{};
  std::optional<std::uint32_t> present{};
  std::optional<std::uint32_t> compute{};
  std::optional<std::uint32_t> transfer{};
}; // struct queue_family_indices

static auto _get_queue_family_indices(const physical_device& physical_device, const surface& surface) -> queue_family_indices {
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

  const auto supports_present = [&](const std::uint32_t family) -> bool {
    auto supported = VkBool32{VK_FALSE};
    validate(vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, family, surface, &supported), "vkGetPhysicalDeviceSurfaceSupportKHR");

    return supported == VK_TRUE;
  };

  // Prefer presenting from the graphics family itself, which avoids sharing swapchain images across
  // families; otherwise any family the window surface actually supports.
  if (supports_present(*result.graphics)) {
    result.present = result.graphics;
  } else {
    for (auto i = std::uint32_t{0}; i < device_queue_family_property_count; ++i) {
      if (device_queue_family_properties[i].queueCount > 0u && supports_present(i)) {
        result.present = i;
        break;
      }
    }
  }

  if (!result.present) {
    throw std::runtime_error("Failed to find a queue family that can present to the window surface");
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

logical_device::logical_device(const physical_device& physical_device, const surface& surface) {
  const auto queue_family_indices = _get_queue_family_indices(physical_device, surface);

  const auto graphics_queue_family_index = queue_family_indices.graphics.value();
  const auto present_queue_family_index = queue_family_indices.present.value();
  const auto compute_queue_family_index = queue_family_indices.compute.value();
  const auto transfer_queue_family_index = queue_family_indices.transfer.value();

  // One queue (index 0) per distinct family; queue types that share a family share that queue and
  // its lock (see _get_queue).
  const auto queue_priority = std::float_t{0.0f};

  auto queue_create_infos = std::vector<VkDeviceQueueCreateInfo>{};

  for (const auto family : {graphics_queue_family_index, present_queue_family_index, compute_queue_family_index, transfer_queue_family_index}) {
    if (std::ranges::any_of(queue_create_infos, [family](const VkDeviceQueueCreateInfo& info) { return info.queueFamilyIndex == family; })) {
      continue;
    }

    auto queue_create_info = VkDeviceQueueCreateInfo{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = family;
    queue_create_info.queueCount = 1u;
    queue_create_info.pQueuePriorities = &queue_priority;

    queue_create_infos.push_back(queue_create_info);
  }

  const auto instance_validation_layers = layers::instance();
  const auto device_extensions = extensions::device();

  const auto available_features = features::query(physical_device);

  auto features = features::enabled(features::required(), features::optional(), available_features);

  _enabled_features = features;

	auto device_create_info = VkDeviceCreateInfo{};
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = &features.chain();
	device_create_info.queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size());
	device_create_info.pQueueCreateInfos = queue_create_infos.data();
  device_create_info.enabledLayerCount = static_cast<std::uint32_t>(instance_validation_layers.size());
  device_create_info.ppEnabledLayerNames = instance_validation_layers.data();
	device_create_info.enabledExtensionCount = static_cast<std::uint32_t>(device_extensions.size());
	device_create_info.ppEnabledExtensionNames = device_extensions.data();
	device_create_info.pEnabledFeatures = nullptr;

	validate(vkCreateDevice(physical_device, &device_create_info, nullptr, &_handle), "vkCreateDevice");

  _get_queue<queue::type::graphics>(graphics_queue_family_index);
  _get_queue<queue::type::present>(present_queue_family_index);
  _get_queue<queue::type::compute>(compute_queue_family_index);
  _get_queue<queue::type::transfer>(transfer_queue_family_index);

  utility::logger<"graphics">::debug("Created logical device with {} unique queues", queue_create_infos.size());
}

logical_device::~logical_device() {
  vkDestroyDevice(_handle, nullptr);
}

auto logical_device::wait_idle() const -> void {
  const auto lock = std::scoped_lock{_queue_mutexes[0], _queue_mutexes[1], _queue_mutexes[2], _queue_mutexes[3]};

  validate(vkDeviceWaitIdle(_handle), "vkDeviceWaitIdle");
}

auto logical_device::_set_debug_name(VkObjectType object_type, std::uint64_t object_handle, const std::string& name) const -> void {
  if constexpr (utility::is_build_type_debug_v) {
    auto* function = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(_handle, "vkSetDebugUtilsObjectNameEXT"));

    if (function) {
      auto name_info = VkDebugUtilsObjectNameInfoEXT{};
      name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
      name_info.objectType = object_type;
      name_info.objectHandle = object_handle;
      name_info.pObjectName = name.c_str();

      std::invoke(function, _handle, &name_info);
    } else {
      utility::logger<"graphics">::warn("Function 'vkSetDebugUtilsObjectNameEXT' could not be found");
    }
  }
}

} // namespace sbx::graphics
