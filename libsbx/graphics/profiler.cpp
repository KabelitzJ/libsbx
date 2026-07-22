// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/profiler.hpp>

#if defined(SBX_ENABLE_PROFILING)

#include <array>

#include <magic_enum/magic_enum.hpp>

#include <libsbx/utility/logger.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics::detail {

static auto contexts = std::array<TracyVkCtx, magic_enum::enum_count<queue::type>()>{};

auto register_gpu_context(const queue::type type, std::string_view name, const instance& instance, const physical_device& physical_device, const logical_device& logical_device) -> void {
  const auto index = utility::to_underlying(type);

  const auto& queue = logical_device.queue(type);

  auto pool_create_info = VkCommandPoolCreateInfo{};
  pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_create_info.queueFamilyIndex = queue.family();

  auto command_pool = VkCommandPool{};
  validate(vkCreateCommandPool(logical_device, &pool_create_info, nullptr, &command_pool), "vkCreateCommandPool");

  auto allocate_info = VkCommandBufferAllocateInfo{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1;

  auto command_buffer = VkCommandBuffer{};
  validate(vkAllocateCommandBuffers(logical_device, &allocate_info, &command_buffer), "vkAllocateCommandBuffers");

  auto* get_time_domains = reinterpret_cast<PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceCalibrateableTimeDomainsEXT"));
  auto* get_calibrated_timestamps = reinterpret_cast<PFN_vkGetCalibratedTimestampsEXT>(vkGetDeviceProcAddr(logical_device, "vkGetCalibratedTimestampsEXT"));

  if (get_time_domains != nullptr && get_calibrated_timestamps != nullptr) {
    contexts[index] = TracyVkContextCalibrated(physical_device, logical_device, queue, command_buffer, get_time_domains, get_calibrated_timestamps);
  } else {
    contexts[index] = TracyVkContext(physical_device, logical_device, queue, command_buffer);

    utility::logger<"graphics">::warn("Calibrated timestamps unavailable; GPU/CPU timelines may drift");
  }

  contexts[index] = TracyVkContext(physical_device, logical_device, queue, command_buffer);

  TracyVkContextName(contexts[index], name.data(), static_cast<std::uint16_t>(name.size()));

  vkFreeCommandBuffers(logical_device, command_pool, 1, &command_buffer);
  vkDestroyCommandPool(logical_device, command_pool, nullptr);
}

auto destroy_gpu_contexts() -> void {
  for (auto& context : contexts) {
    if (context) {
      TracyVkDestroy(context);
      context = nullptr;
    }
  }
}

auto gpu_context(const queue::type type) noexcept -> TracyVkCtx {
  return contexts[utility::to_underlying(type)];
}

} // namespace sbx::graphics::detail

#endif // SBX_ENABLE_PROFILING
