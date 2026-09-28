// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/graphics/frame_context.hpp>

#include <array>
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

  graphics_module.logical_device().wait_idle();

  // Semaphores release themselves (RAII members) once the device is idle.
  _command_buffers.clear();
  _swapchain.reset();
}

auto frame_context::_initialize() -> void {
  _timeline.emplace(semaphore::type::timeline, "Frame Timeline");

  _image_available.reserve(swapchain::max_frames_in_flight);

  for (auto slot = std::uint32_t{0u}; slot < swapchain::max_frames_in_flight; ++slot) {
    _image_available.emplace_back(semaphore::type::binary, fmt::format("Image Available {}", slot));
  }

  _command_buffers.reserve(swapchain::max_frames_in_flight);

  for (auto slot = std::uint32_t{0u}; slot < swapchain::max_frames_in_flight; ++slot) {
    _command_buffers.emplace_back(queue::type::graphics, false);
  }

  _recreate_swapchain();

  _is_initialized.store(true, std::memory_order_release);
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

  // Never let the host run more than max_frames_in_flight ahead of the device.
  if (const auto index = frame_index(); index > swapchain::max_frames_in_flight) {
    SBX_PROFILE_SCOPE("frame_context::wait_timeline");
    _timeline->wait(index - swapchain::max_frames_in_flight);
  }

  const auto completed_value = _timeline->value();
  _timeline_value.store(completed_value, std::memory_order_release);

  // Everything retired at or before this value is no longer referenced by the device.
  auto& resource_registry = graphics_module.resource_registry();
  resource_registry.collect_all(completed_value);

  auto& bindless_table = graphics_module.bindless_table();
  bindless_table.collect(completed_value);
  bindless_table.flush_writes();

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

  const auto slot = _slot();
  const auto index = frame_index();

  auto& command_buffer = _command_buffers[slot];

  command_buffer.end();

  const auto image_index = _swapchain->active_image_index();

  // _image_available is binary, paired with dummy value 0 — valid per spec to mix binary and timeline semaphores in one wait array.
  auto wait_semaphores = std::vector<VkSemaphore>{_image_available[slot]};
  auto wait_stages = std::vector<VkPipelineStageFlags>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  auto wait_values = std::vector<std::uint64_t>{0u};

  // Waiting on an already-signaled value is free, so there's no need to track which frames have
  // waited on what: every frame waits for all async work published so far.
  if (const auto async_value = _async_published.load(std::memory_order_acquire); async_value != 0u) {
    wait_semaphores.push_back(*_async_timeline);
    wait_stages.push_back(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    wait_values.push_back(async_value);
  }

  const auto command_buffers = std::array<VkCommandBuffer, 1u>{command_buffer.handle()};

  // The value paired with the binary semaphore is ignored, but the arrays must stay parallel.
  const auto signal_semaphores = std::array<VkSemaphore, 2u>{_render_finished[image_index], *_timeline};
  const auto signal_values = std::array<std::uint64_t, 2u>{0u, index};

  auto timeline_submit_info = VkTimelineSemaphoreSubmitInfo{};
  timeline_submit_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
  timeline_submit_info.waitSemaphoreValueCount = static_cast<std::uint32_t>(wait_values.size());
  timeline_submit_info.pWaitSemaphoreValues = wait_values.data();
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

  {
    const auto lock = graphics_queue.lock();
    validate(vkQueueSubmit(graphics_queue, 1u, &submit_info, VK_NULL_HANDLE), "vkQueueSubmit");
  }

  // The submit is in flight and will signal this value, so the frame is spent either way.
  _frame_index.fetch_add(1u, std::memory_order_release);

  const auto present_result = _swapchain->present(_render_finished[image_index]);

  if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
    _recreate_swapchain();
  } else if (present_result != VK_SUCCESS) {
    throw std::runtime_error{"Failed to present swapchain image"};
  }
}

auto frame_context::submit_async(command_buffer& commands, const fence& fence) -> void {
  auto lock = std::lock_guard{_async_mutex};

  if (!_async_timeline) {
    _async_timeline.emplace(semaphore::type::timeline, "Async Compute Timeline");
  }

  const auto value = _async_published.load(std::memory_order_relaxed) + 1u;

  commands.submit({}, *_async_timeline, fence, value);

  // Published only after the submit went through, so a frame never waits on a value nothing will signal.
  _async_published.store(value, std::memory_order_release);
}

auto frame_context::_recreate_swapchain() -> void {
  SBX_PROFILE_SCOPE("frame_context::_recreate_swapchain");

  auto& graphics_module = core::engine::get_module<graphics::graphics_module>();

  graphics_module.logical_device().wait_idle();

  _swapchain = std::make_unique<graphics::swapchain>(_swapchain);

  _recreate_per_image_semaphores();
}

auto frame_context::_recreate_per_image_semaphores() -> void {
  const auto image_count = _swapchain->image_count();

  _render_finished.clear();
  _render_finished.reserve(image_count);

  for (auto index = std::uint32_t{0u}; index < image_count; ++index) {
    _render_finished.emplace_back(semaphore::type::binary, fmt::format("Render Finished {}", index));
  }
}

} // namespace sbx::graphics
