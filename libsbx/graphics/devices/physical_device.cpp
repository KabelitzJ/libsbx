// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/physical_device.hpp>

#include <vulkan/vulkan.h>

#include <libsbx/utility/logger.hpp>
#include <libsbx/utility/exception.hpp>
#include <libsbx/utility/iterator.hpp>

#include <libsbx/graphics/validate.hpp>

#include <libsbx/graphics/devices/features.hpp>

namespace sbx::graphics {

static auto _score(const VkPhysicalDeviceProperties& properties) -> std::uint32_t {
  auto score = std::uint32_t{0};

  switch (properties.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: {
      score += 1000u;
      break;
    }
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: {
      score += 100u;
      break;
    }
    default: {
      break;
    }
  }

  return score;
}

physical_device::physical_device(const instance& instance) {
  auto count = std::uint32_t{0};
  validate(vkEnumeratePhysicalDevices(instance, &count, nullptr), "vkEnumeratePhysicalDevices");

  if (count == 0u) {
    throw utility::runtime_error{"No vulkan capable devices found"};
  }

  auto devices = utility::make_vector<VkPhysicalDevice>(count);
  validate(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");

  const auto required_features = features::required();

  auto best_score = std::uint32_t{0};

  for (const auto& candidate : devices) {
    auto properties = VkPhysicalDeviceProperties{};
    vkGetPhysicalDeviceProperties(candidate, &properties);

    utility::logger<"graphics">::info("Device: {}", std::string_view{properties.deviceName});

    if (properties.apiVersion < VK_API_VERSION_1_4) {
      utility::logger<"graphics">::debug("Skipping '{}': api version below 1.4", std::string_view{properties.deviceName});

      continue;
    }

    if (!features::query(candidate).supports(required_features)) {
      utility::logger<"graphics">::debug("Skipping '{}': missing required features", std::string_view{properties.deviceName});

      continue;
    }

    const auto score = _score(properties);

    if (score > best_score) {
      best_score = score;
      _handle = candidate;
      _properties = properties;
    }
  }

  if (!_handle) {
    throw utility::runtime_error{"No device supports the required vulkan 1.4 feature set"};
  }

  auto properties = VkPhysicalDeviceProperties{};
  vkGetPhysicalDeviceProperties(_handle, &properties);

  utility::logger<"graphics">::info("Device: {}", std::string_view{properties.deviceName});

  vkGetPhysicalDeviceMemoryProperties(_handle, &_memory_properties);

  auto family_count = std::uint32_t{0};
  vkGetPhysicalDeviceQueueFamilyProperties(_handle, &family_count, nullptr);

  _queue_families.resize(family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(_handle, &family_count, _queue_families.data());
}

auto physical_device::device_local_memory() const noexcept -> VkDeviceSize {
  auto total = VkDeviceSize{0};

  for (auto i = std::uint32_t{0}; i < _memory_properties.memoryHeapCount; ++i) {
    const auto& heap = _memory_properties.memoryHeaps[i];

    if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
      total += heap.size;
    }
  }

  return total;
}

} // namespace sbx::graphics
