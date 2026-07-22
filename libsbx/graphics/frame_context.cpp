// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/frame_context.hpp>

#include <limits>
#include <stdexcept>

#include <libsbx/utility/assert.hpp>
#include <libsbx/utility/logger.hpp>

#include <libsbx/core/engine.hpp>

#include <libsbx/graphics/graphics_module.hpp>
#include <libsbx/graphics/profiler.hpp>
#include <libsbx/graphics/validate.hpp>

namespace sbx::graphics {

frame_context::~frame_context() {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  logical_device.wait_idle();

  _command_buffers.clear();

  _destroy_per_image_semaphores();

  for (auto& semaphore : _image_available) {
    vkDestroySemaphore(logical_device, semaphore, nullptr);
    semaphore = VK_NULL_HANDLE;
  }

  vkDestroySemaphore(logical_device, _timeline, nullptr);
  _timeline = VK_NULL_HANDLE;

  _swapchain.reset();
}

auto frame_context::_initialize() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  auto type_create_info = VkSemaphoreTypeCreateInfo{};
  type_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
  type_create_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
  type_create_info.initialValue = 0u;

  auto timeline_create_info = VkSemaphoreCreateInfo{};
  timeline_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  timeline_create_info.pNext = &type_create_info;

  validate(vkCreateSemaphore(logical_device, &timeline_create_info, nullptr, &_timeline), "vkCreateSemaphore");

  logical_device.set_debug_name(_timeline, "Frame Timeline");

  auto semaphore_create_info = VkSemaphoreCreateInfo{};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (auto slot = std::uint32_t{0u}; slot < swapchain::max_frames_in_flight; ++slot) {
    validate(vkCreateSemaphore(logical_device, &semaphore_create_info, nullptr, &_image_available[slot]), "vkCreateSemaphore");

    logical_device.set_debug_name(_image_available[slot], fmt::format("Image Available {}", slot));
  }

  _command_buffers.reserve(swapchain::max_frames_in_flight);

  for (auto slot = std::uint32_t{0u}; slot < swapchain::max_frames_in_flight; ++slot) {
    _command_buffers.emplace_back(queue::type::graphics, false);
  }

  _recreate_swapchain();
}

auto frame_context::begin_frame() -> memory::observer_ptr<command_buffer> {
  SBX_PROFILE_SCOPE("frame_context::begin_frame");

  if (!is_initialized()) {
    _initialize();
  }

  if (_swapchain->is_outdated()) {
    _recreate_swapchain();
  }

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  // Never let the host run more than max_frames_in_flight ahead of the device.
  if (_frame_index > swapchain::max_frames_in_flight) {
    _wait_timeline(_frame_index - swapchain::max_frames_in_flight);
  }

  validate(vkGetSemaphoreCounterValue(logical_device, _timeline, &_timeline_value), "vkGetSemaphoreCounterValue");

  // Everything retired at or before this value is no longer referenced by the device.
  auto& resource_registry = graphics_module.resource_registry();
  resource_registry.collect_all(_timeline_value);

  const auto slot = _slot();

  const auto acquire_result = _swapchain->acquire_next_image(_image_available[slot]);

  if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
    _recreate_swapchain();

    return nullptr;
  }

  if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error{"Failed to acquire swapchain image"};
  }

  auto& command_buffer = _command_buffers[slot];

  command_buffer.reset();
  command_buffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  SBX_PROFILE_GPU_COLLECT(command_buffer);

  return memory::make_observer(command_buffer);
}

auto frame_context::end_frame() -> void {
  SBX_PROFILE_SCOPE("frame_context::end_frame");

  utility::assert_that(is_initialized(), "Called end_frame without a matching begin_frame");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  auto& command_buffer = _command_buffers[_slot()];

  command_buffer.end();

  const auto image_index = _swapchain->active_image_index();

  const auto wait_semaphores = std::array<VkSemaphore, 1u>{_image_available[_slot()]};
  const auto wait_stages = std::array<VkPipelineStageFlags, 1u>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  const auto command_buffers = std::array<VkCommandBuffer, 1u>{command_buffer.handle()};

  // The value paired with the binary semaphore is ignored, but the arrays must stay parallel.
  const auto signal_semaphores = std::array<VkSemaphore, 2u>{_render_finished[image_index], _timeline};
  const auto signal_values = std::array<std::uint64_t, 2u>{0u, _frame_index};

  auto timeline_submit_info = VkTimelineSemaphoreSubmitInfo{};
  timeline_submit_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timeline_submit_info.signalSemaphoreValueCount = static_cast<std::uint32_t>(signal_values.size());
  timeline_submit_info.pSignalSemaphoreValues = signal_values.data();

  auto submit_info = VkSubmitInfo{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = &timeline_submit_info;
  submit_info.waitSemaphoreCount = static_cast<std::uint32_t>(wait_semaphores.size());
  submit_info.pWaitSemaphores = wait_semaphores.data();
  submit_info.pWaitDstStageMask = wait_stages.data();
  submit_info.commandBufferCount = static_cast<std::uint32_t>(command_buffers.size());
  submit_info.pCommandBuffers = command_buffers.data();
  submit_info.signalSemaphoreCount = static_cast<std::uint32_t>(signal_semaphores.size());
  submit_info.pSignalSemaphores = signal_semaphores.data();

  const auto& graphics_queue = logical_device.queue<queue::type::graphics>();

  validate(vkQueueSubmit(graphics_queue, 1u, &submit_info, VK_NULL_HANDLE), "vkQueueSubmit");

  // The submit is in flight and will signal this value, so the frame is spent either way.
  ++_frame_index;

  const auto present_result = _swapchain->present(_render_finished[image_index]);

  if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
    _recreate_swapchain();
  } else if (present_result != VK_SUCCESS) {
    throw std::runtime_error{"Failed to present swapchain image"};
  }
}

auto frame_context::_recreate_swapchain() -> void {
  SBX_PROFILE_SCOPE("frame_context::_recreate_swapchain");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  logical_device.wait_idle();

  _swapchain = std::make_unique<graphics::swapchain>(_swapchain);

  _recreate_per_image_semaphores();
}

auto frame_context::_recreate_per_image_semaphores() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  _destroy_per_image_semaphores();

  const auto image_count = _swapchain->image_count();

  _render_finished.resize(image_count);

  auto semaphore_create_info = VkSemaphoreCreateInfo{};
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (auto index = std::uint32_t{0u}; index < image_count; ++index) {
    validate(vkCreateSemaphore(logical_device, &semaphore_create_info, nullptr, &_render_finished[index]), "vkCreateSemaphore");

    logical_device.set_debug_name(_render_finished[index], fmt::format("Render Finished {}", index));
  }
}

auto frame_context::_destroy_per_image_semaphores() -> void {
  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  for (auto& semaphore : _render_finished) {
    vkDestroySemaphore(logical_device, semaphore, nullptr);
  }

  _render_finished.clear();
}

auto frame_context::_wait_timeline(const std::uint64_t value) const -> void {
  SBX_PROFILE_SCOPE("frame_context::_wait_timeline");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  const auto& logical_device = graphics_module.logical_device();

  auto wait_info = VkSemaphoreWaitInfo{};
  wait_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
  wait_info.semaphoreCount = 1u;
  wait_info.pSemaphores = &_timeline;
  wait_info.pValues = &value;

  validate(vkWaitSemaphores(logical_device, &wait_info, std::numeric_limits<std::uint64_t>::max()), "vkWaitSemaphores");
}

} // namespace sbx::graphics
