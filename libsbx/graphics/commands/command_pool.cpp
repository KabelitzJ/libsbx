// SPDX-License-Identifier: MIT
#include <libsbx/graphics/commands/command_pool.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>

namespace sbx::graphics {

command_pool::command_pool(const queue::type queue_type)
: _queue_type{queue_type} {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();
  
  const auto& queue = logical_device.queue(queue_type);

  auto command_pool_create_info = VkCommandPoolCreateInfo{};
  command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  command_pool_create_info.queueFamilyIndex = queue.family();

  validate(vkCreateCommandPool(logical_device, &command_pool_create_info, nullptr, &_handle));
}

command_pool::~command_pool() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  auto& logical_device = graphics_module.logical_device();
  vkDestroyCommandPool(logical_device, _handle, nullptr);
}

auto command_pool::handle() const noexcept -> const VkCommandPool& {
  return _handle;
}

command_pool::operator const VkCommandPool&() const noexcept {
  return _handle;
}

auto command_pool::type() const noexcept -> queue::type {
  return _queue_type;
}

} // namespace sbx::graphics
