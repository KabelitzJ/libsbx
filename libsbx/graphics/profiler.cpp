// SPDX-License-Identifier: MIT
#include <libsbx/graphics/profiler.hpp>

#if defined(SBX_PROFILING_ENABLED)

#include <array>

#include <magic_enum/magic_enum.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics::detail {

auto contexts = std::array<TracyVkCtx, magic_enum::enum_count<queue::type>()>{};

auto register_gpu_context(const queue::type type, const char* name, std::uint16_t size, const physical_device& physical_device, const logical_device& logical_device) -> void {
  const auto index = utility::to_underlying(type);

  const auto& queue = logical_device.queue(type);

  auto pool_create_info = VkCommandPoolCreateInfo{};
  pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_create_info.queueFamilyIndex = queue.family();

  auto command_pool = VkCommandPool{};
  validate(vkCreateCommandPool(logical_device, &pool_create_info, nullptr, &command_pool));

  auto allocate_info = VkCommandBufferAllocateInfo{};
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.commandPool = command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1;

  auto command_buffer = VkCommandBuffer{};
  validate(vkAllocateCommandBuffers(logical_device, &allocate_info, &command_buffer));

  contexts[index] = TracyVkContext(physical_device, logical_device, queue, command_buffer);
  TracyVkContextName(contexts[index], name, size);

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

#endif // SBX_PROFILING_ENABLED
