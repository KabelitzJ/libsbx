// SPDX-License-Identifier: MIT
#include <libsbx/graphics/devices/logical_device.hpp>

#include <optional>
#include <unordered_set>
#include <vector>

#include <libsbx/utility/logger.hpp>

#include <libsbx/graphics/devices/features.hpp>

namespace sbx::graphics {

struct queue_family_selection {
  std::uint32_t graphics{};
  std::uint32_t compute{};
  std::uint32_t transfer{};
}; // struct queue_family_selection

static auto _select_queue_families(const std::vector<VkQueueFamilyProperties>& families) -> queue_family_selection {
  auto graphics = std::optional<std::uint32_t>{};
  auto dedicated_compute = std::optional<std::uint32_t>{};
  auto dedicated_transfer = std::optional<std::uint32_t>{};

  for (auto i = std::uint32_t{0}; i < static_cast<std::uint32_t>(families.size()); ++i) {
    const auto flags = families[i].queueFlags;

    if (!graphics && (flags & VK_QUEUE_GRAPHICS_BIT) && (flags & VK_QUEUE_COMPUTE_BIT)) {
      graphics = i;
    } else if (!dedicated_compute && (flags & VK_QUEUE_COMPUTE_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT)) {
      dedicated_compute = i;
    } else if (!dedicated_transfer && (flags & VK_QUEUE_TRANSFER_BIT) && !(flags & VK_QUEUE_GRAPHICS_BIT) && !(flags & VK_QUEUE_COMPUTE_BIT)) {
      dedicated_transfer = i;
    }
  }

  if (!graphics) {
    throw utility::runtime_error{"No graphics capable queue family found"};
  }

  return queue_family_selection{
    .graphics = *graphics,
    .compute = dedicated_compute.value_or(*graphics),
    .transfer = dedicated_transfer.value_or(*graphics)
  };
}

logical_device::logical_device(const physical_device& physical_device) {
  const auto selection = _select_queue_families(physical_device.queue_families());

  const auto unique_families = std::unordered_set<std::uint32_t>{selection.graphics, selection.compute, selection.transfer};

  constexpr auto priority = 1.0f;

  auto queue_create_infos = std::vector<VkDeviceQueueCreateInfo>{};

  for (const auto family : unique_families) {
    auto queue_create_info = VkDeviceQueueCreateInfo{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = family;
    queue_create_info.queueCount = 1u;
    queue_create_info.pQueuePriorities = &priority;

    queue_create_infos.push_back(queue_create_info);
  }

  auto extensions = std::vector<const char*>{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  // required ∪ (optional ∩ available) — the old engine's enable-if-available
  // behavior, expressed as one merge instead of forty if-statements.
  const auto available = device_features::query(physical_device);

  auto features = device_features::enabled(device_features::required(), device_features::optional(), available);

  // Extension features may only be enabled together with their extension.
  if (features.compute_shader_derivatives().computeDerivativeGroupQuads || features.compute_shader_derivatives().computeDerivativeGroupLinear) {
    extensions.push_back(VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME);
  }

  auto create_info = VkDeviceCreateInfo{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pNext = &features.chain();
  create_info.queueCreateInfoCount = static_cast<std::uint32_t>(queue_create_infos.size());
  create_info.pQueueCreateInfos = queue_create_infos.data();
  create_info.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
  create_info.ppEnabledExtensionNames = extensions.data();

  validate(vkCreateDevice(physical_device, &create_info, nullptr, &_handle), "vkCreateDevice");

  _graphics_queue.family = selection.graphics;
  _compute_queue.family = selection.compute;
  _transfer_queue.family = selection.transfer;

  vkGetDeviceQueue(_handle, _graphics_queue.family, 0u, &_graphics_queue.handle);
  vkGetDeviceQueue(_handle, _compute_queue.family, 0u, &_compute_queue.handle);
  vkGetDeviceQueue(_handle, _transfer_queue.family, 0u, &_transfer_queue.handle);

#if defined(SBX_BUILD_TYPE_DEBUG)
  _set_object_name_function = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetDeviceProcAddr(_handle, "vkSetDebugUtilsObjectNameEXT"));
#endif
}

logical_device::~logical_device() {
  vkDestroyDevice(_handle, nullptr);
}

auto logical_device::wait_idle() const -> void {
  validate(vkDeviceWaitIdle(_handle), "vkDeviceWaitIdle");
}

#if defined(SBX_BUILD_TYPE_DEBUG)

auto logical_device::set_debug_name(VkObjectType object_type, std::uint64_t object_handle, const std::string& name) const -> void {
  if (!_set_object_name_function) {
    return;
  }

  auto name_info = VkDebugUtilsObjectNameInfoEXT{};
  name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
  name_info.objectType = object_type;
  name_info.objectHandle = object_handle;
  name_info.pObjectName = name.c_str();

  _set_object_name_function(_handle, &name_info);
}

#endif // SBX_BUILD_TYPE_DEBUG

} // namespace sbx::graphics
