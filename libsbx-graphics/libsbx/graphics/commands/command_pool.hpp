// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_COMMANDS_COMMAND_POOL_HPP_
#define LIBSBX_GRAPHICS_COMMANDS_COMMAND_POOL_HPP_

#include <thread>

#include <vulkan/vulkan.hpp>

#include <libsbx/graphics/devices/logical_device.hpp>

namespace sbx::graphics {

class command_pool {

public:

  command_pool(VkQueueFlagBits queue_type = VK_QUEUE_GRAPHICS_BIT);

  ~command_pool();

  auto handle() const noexcept -> const VkCommandPool&;

  operator const VkCommandPool&() const noexcept;

private:

  auto _queue(VkQueueFlagBits queue_type) const -> const queue&;

  VkCommandPool _handle{};

}; // class command_pool

} // namespace sbs::graphics

#endif // LIBSBX_GRAPHICS_COMMANDS_COMMAND_POOL_HPP_
