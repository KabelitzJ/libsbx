// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/commands/fence.hpp>

#include <limits>
#include <utility>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

fence::fence(const bool is_signaled, const std::string& name) {
  const auto& logical_device = core::engine::get_module<graphics_module>().logical_device();

  auto create_info = VkFenceCreateInfo{};
  create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  create_info.flags = is_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : VkFenceCreateFlags{0u};

  validate(vkCreateFence(logical_device, &create_info, nullptr, &_handle), "vkCreateFence");

  if (!name.empty()) {
    logical_device.set_debug_name(_handle, name);
  }
}

fence::fence(fence&& other) noexcept
: _handle{std::exchange(other._handle, VK_NULL_HANDLE)} { }

fence::~fence() {
  _destroy();
}

auto fence::operator=(fence&& other) noexcept -> fence& {
  if (this != &other) {
    _destroy();

    _handle = std::exchange(other._handle, VK_NULL_HANDLE);
  }

  return *this;
}

auto fence::is_signaled() const -> bool {
  const auto& logical_device = core::engine::get_module<graphics_module>().logical_device();

  return vkGetFenceStatus(logical_device, _handle) == VK_SUCCESS;
}

auto fence::wait() const -> void {
  const auto& logical_device = core::engine::get_module<graphics_module>().logical_device();

  validate(vkWaitForFences(logical_device, 1u, &_handle, VK_TRUE, std::numeric_limits<std::uint64_t>::max()), "vkWaitForFences");
}

auto fence::reset() -> void {
  const auto& logical_device = core::engine::get_module<graphics_module>().logical_device();

  validate(vkResetFences(logical_device, 1u, &_handle), "vkResetFences");
}

auto fence::_destroy() noexcept -> void {
  if (_handle != VK_NULL_HANDLE) {
    const auto& logical_device = core::engine::get_module<graphics_module>().logical_device();

    vkDestroyFence(logical_device, _handle, nullptr);
    _handle = VK_NULL_HANDLE;
  }
}

} // namespace sbx::graphics
