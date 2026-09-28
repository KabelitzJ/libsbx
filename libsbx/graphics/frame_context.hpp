// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_FRAME_CONTEXT_HPP_
#define LIBSBX_GRAPHICS_FRAME_CONTEXT_HPP_

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/commands/fence.hpp>
#include <libsbx/graphics/commands/semaphore.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>

namespace sbx::graphics {

class frame_context : public utility::noncopyable {

public:

  frame_context() = default;

  ~frame_context();

  [[nodiscard]] auto begin_frame() -> memory::observer_ptr<command_buffer>;

  auto end_frame() -> void;

  // frame_index/timeline_value/is_initialized are read from other threads too (asset release,
  // residency checks, editor panels) while the render thread advances them, hence the atomics.

  [[nodiscard]] auto frame_index() const noexcept -> std::uint64_t {
    return _frame_index.load(std::memory_order_acquire);
  }

  [[nodiscard]] auto timeline_value() const noexcept -> std::uint64_t {
    return _timeline_value.load(std::memory_order_acquire);
  }

  [[nodiscard]] auto is_initialized() const noexcept -> bool {
    return _is_initialized.load(std::memory_order_acquire);
  }

  [[nodiscard]] auto swapchain() const noexcept -> const graphics::swapchain& {
    return *_swapchain;
  }

  /**
   * @brief Submits GPU work that runs outside the frame without a host wait (script
   * ComputeCommands.SubmitAsync) so that it signals the async timeline: every frame submitted after
   * this waits on it at the vertex/fragment/compute stages (see end_frame), so rendering never
   * samples what that work is still writing. @p fence is signaled too, for host polling.
   * Thread-safe; submissions are serialized so timeline values are signaled in increasing order.
   */
  auto submit_async(command_buffer& commands, const fence& fence) -> void;

private:

  auto _initialize() -> void;

  auto _recreate_swapchain() -> void;

  auto _recreate_per_image_semaphores() -> void;

  [[nodiscard]] auto _slot() const noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(frame_index() % swapchain::max_frames_in_flight);
  }

  std::optional<semaphore> _timeline{};
  std::atomic<std::uint64_t> _timeline_value{0u};
  std::atomic<bool> _is_initialized{false};

  std::atomic<std::uint64_t> _frame_index{1u};

  std::unique_ptr<graphics::swapchain> _swapchain{};

  std::vector<semaphore> _image_available{};
  std::vector<semaphore> _render_finished{};

  std::vector<command_buffer> _command_buffers{};

  // See submit_async. Created lazily on the first async submit; _async_published is read lock-free
  // by end_frame on the render thread (and orders the read of _async_timeline after its creation).
  std::mutex _async_mutex{};
  std::optional<semaphore> _async_timeline{};
  std::atomic<std::uint64_t> _async_published{0u};

}; // class frame_context

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_FRAME_CONTEXT_HPP_
