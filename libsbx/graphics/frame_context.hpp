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

class frame_context : public utility::noncopyable {

public:

  frame_context() = default;

  ~frame_context();

  [[nodiscard]] auto begin_frame() -> memory::observer_ptr<command_buffer>;

  auto end_frame() -> void;

  [[nodiscard]] auto frame_index() const noexcept -> std::uint64_t {
    return _frame_index;
  }

  [[nodiscard]] auto timeline_value() const noexcept -> std::uint64_t {
    return _timeline_value;
  }

  [[nodiscard]] auto is_initialized() const noexcept -> bool {
    return _timeline != VK_NULL_HANDLE;
  }

  [[nodiscard]] auto swapchain() const noexcept -> const graphics::swapchain& {
    return *_swapchain;
  }

  [[nodiscard]] auto timeline() const noexcept -> VkSemaphore {
    return _timeline;
  }

  [[nodiscard]] auto previous_frame_value() const noexcept -> std::uint64_t {
    return _frame_index - 1u;
  }

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
