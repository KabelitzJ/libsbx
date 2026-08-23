// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_FRAME_CONTEXT_HPP_
#define LIBSBX_GRAPHICS_FRAME_CONTEXT_HPP_

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/memory/observer_ptr.hpp>

#include <libsbx/graphics/commands/command_buffer.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>

namespace sbx::graphics {

/**
 * @brief Owns the frame loop: the timeline semaphore, the swapchain, and everything sized by it.
 *
 * Frame pacing runs on a single timeline semaphore rather than per frame fences. The frame index
 * is monotonic and starts at one, and the submit for frame N signals the timeline value N. That
 * makes "has the GPU finished frame N" a comparison against
 * @ref completed_value, which is what the deferred destruction in `resource_pool` needs and what
 * a fence cannot provide without being tracked per submit.
 *
 * Binary semaphores survive only where the swapchain extension requires them, which is the image
 * acquire and the present.
 *
 * The frame index must never be reset. A timeline signal has to strictly increase, so restarting
 * the counter on a swapchain recreation would make the next submit invalid and hang the throttle.
 */
class frame_context : public utility::noncopyable {

public:

  frame_context() = default;

  ~frame_context();

  /**
   * @brief Waits for the GPU to catch up, reclaims retired resources, acquires an image and opens
   * the frame's command buffer.
   *
   * @return The command buffer to record into, or null when the frame must be skipped because the
   * window is iconified or the swapchain went out of date during the acquire. When null is
   * returned @ref end_frame must not be called, since nothing was submitted and the frame index
   * must not advance past a value the GPU will never signal.
   */
  [[nodiscard]] auto begin_frame() -> memory::observer_ptr<command_buffer>;

  /**
   * @brief Closes the command buffer, submits it signalling the timeline, and presents.
   *
   * Only valid after a @ref begin_frame that returned a command buffer.
   */
  auto end_frame() -> void;

  /**
   * @brief The value the frame currently being recorded will signal once its submit completes.
   *
   * This is the value to pass to `resource_pool::retire`. A resource retired during frame N can
   * only have been referenced by frame N or earlier, so waiting for N is always safe.
   */
  [[nodiscard]] auto frame_index() const noexcept -> std::uint64_t {
    return _frame_index;
  }

  /**
   * @brief The highest timeline value the GPU has signalled, sampled once at the start of the
   * current frame so that every pool observes the same value.
   */
  [[nodiscard]] auto timeline_value() const noexcept -> std::uint64_t {
    return _timeline_value;
  }

  [[nodiscard]] auto is_initialized() const noexcept -> bool {
    return _timeline != VK_NULL_HANDLE;
  }

  [[nodiscard]] auto swapchain() const noexcept -> const graphics::swapchain& {
    return *_swapchain;
  }

  /**
   * @brief Queues an extra timeline wait for the next @ref end_frame's submit, alongside its
   * existing binary `_image_available` wait. Consumed and cleared inside @ref end_frame.
   *
   * For a producer that writes buffers this frame's main command buffer reads later the same
   * frame, but that runs on its own command buffer/submission outside the normal pass list (e.g.
   * particle_simulate_pass) — same-queue submission order alone does not guarantee the main
   * submission's reads happen after such a producer's writes complete (see
   * particle_simulate_pass.hpp for the full reasoning). Call once per producer per frame, before
   * @ref end_frame runs.
   */
  auto add_wait(VkSemaphore semaphore, std::uint64_t value, VkPipelineStageFlags stage) -> void {
    _extra_waits.push_back(extra_wait{semaphore, value, stage});
  }

private:

  struct extra_wait {
    VkSemaphore semaphore;
    std::uint64_t value;
    VkPipelineStageFlags stage;
  }; // struct extra_wait

  auto _initialize() -> void;

  auto _recreate_swapchain() -> void;

  auto _recreate_per_image_semaphores() -> void;

  auto _destroy_per_image_semaphores() -> void;

  auto _wait_timeline(std::uint64_t value) const -> void;

  [[nodiscard]] auto _slot() const noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(_frame_index % swapchain::max_frames_in_flight);
  }

  VkSemaphore _timeline{};
  std::uint64_t _timeline_value{0u};

  std::uint64_t _frame_index{1u};

  std::unique_ptr<graphics::swapchain> _swapchain{};

  std::array<VkSemaphore, swapchain::max_frames_in_flight> _image_available{};
  std::vector<VkSemaphore> _render_finished{};

  std::vector<command_buffer> _command_buffers{};

  std::vector<extra_wait> _extra_waits{};

}; // class frame_context

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_FRAME_CONTEXT_HPP_
