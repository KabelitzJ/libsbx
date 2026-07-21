// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_COMMANDS_COMMAND_POOL_HPP_
#define LIBSBX_GRAPHICS_COMMANDS_COMMAND_POOL_HPP_

#include <thread>

#include <vulkan/vulkan.h>

#include <libsbx/graphics/devices/logical_device.hpp>

namespace sbx::graphics {

class command_pool {

public:

  command_pool(const queue::type queue_type);

  ~command_pool();

  auto handle() const noexcept -> const VkCommandPool&;

  operator const VkCommandPool&() const noexcept;

  auto type() const noexcept -> queue::type;

private:

  queue::type _queue_type{};
  VkCommandPool _handle{};

}; // class command_pool

} // namespace sbs::graphics

#endif // LIBSBX_GRAPHICS_COMMANDS_COMMAND_POOL_HPP_
