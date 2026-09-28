// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/commands/semaphore.hpp>

#include <limits>
#include <utility>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

semaphore::semaphore(const type kind, const std::string& name) {
  const auto& logical_device = core::engine::get_module<graphics_module>().logical_device();

  auto type_create_info = VkSemaphoreTypeCreateInfo{};
  type_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  type_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  type_create_info.initialValue = 0u;

  auto create_info = VkSemaphoreCreateInfo{};
  create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  create_info.pNext = (kind == type::timeline) ? &type_create_info : nullptr;

  validate(vkCreateSemaphore(logical_device, &create_info, nullptr, &_handle), "vkCreateSemaphore");

  if (!name.empty()) {
    logical_device.set_debug_name(_handle, name);
  }
}

semaphore::semaphore(semaphore&& other) noexcept
: _handle{std::exchange(other._handle, VK_NULL_HANDLE)} { }

semaphore::~semaphore() {
  _destroy();
}

auto semaphore::operator=(semaphore&& other) noexcept -> semaphore& {
  if (this != &other) {
    _destroy();

    _handle = std::exchange(other._handle, VK_NULL_HANDLE);
  }

  return *this;
}

auto semaphore::value() const -> std::uint64_t {
  const auto& logical_device = core::engine::get_module<graphics_module>().logical_device();

  auto result = std::uint64_t{0u};

  validate(vkGetSemaphoreCounterValue(logical_device, _handle, &result), "vkGetSemaphoreCounterValue");

  return result;
}

auto semaphore::wait(const std::uint64_t value) const -> void {
  const auto& logical_device = core::engine::get_module<graphics_module>().logical_device();

  auto wait_info = VkSemaphoreWaitInfo{};
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wait_info.semaphoreCount = 1u;
  wait_info.pSemaphores = &_handle;
  wait_info.pValues = &value;

  validate(vkWaitSemaphores(logical_device, &wait_info, std::numeric_limits<std::uint64_t>::max()), "vkWaitSemaphores");
}

auto semaphore::_destroy() noexcept -> void {
  if (_handle != VK_NULL_HANDLE) {
    const auto& logical_device = core::engine::get_module<graphics_module>().logical_device();

    vkDestroySemaphore(logical_device, _handle, nullptr);
    _handle = VK_NULL_HANDLE;
  }
}

} // namespace sbx::graphics
