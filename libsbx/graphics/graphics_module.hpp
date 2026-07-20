// SPDX-License-Identifier: MIT
#ifndef LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_
#define LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_

#include <memory>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/graphics/commands/command_pool.hpp>
#include <libsbx/graphics/commands/command_buffer.hpp>

#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>
#include <libsbx/graphics/devices/allocator.hpp>
#include <libsbx/graphics/devices/surface.hpp>
#include <libsbx/graphics/devices/swapchain.hpp>

namespace sbx::graphics {

class graphics_module : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<platform::platform_module>;

  graphics_module();

  ~graphics_module();

  auto update() -> void;

  [[nodiscard]] auto instance() noexcept -> graphics::instance& {
    return _instance;
  }

  [[nodiscard]] auto physical_device() noexcept -> graphics::physical_device& {
    return _physical_device;
  }

  [[nodiscard]] auto logical_device() noexcept -> graphics::logical_device& {
    return _logical_device;
  }

  [[nodiscard]] auto allocator() noexcept -> graphics::allocator& {
    return _allocator;
  }

  [[nodiscard]] auto surface() noexcept -> graphics::surface& {
    return _surface;
  }

  [[nodiscard]] auto swapchain() noexcept -> graphics::swapchain& {
    return *_swapchain;
  }

  auto command_pool(const queue::type type, const std::thread::id& thread_id = std::this_thread::get_id()) -> const std::shared_ptr<command_pool>&;

private:

  struct per_frame_data {
    // graphics
    VkSemaphore image_available_semaphore{nullptr};
    VkFence graphics_in_flight_fence{nullptr};
    // compute
    VkSemaphore compute_finished_semaphore{nullptr};
    VkFence compute_in_flight_fence{nullptr};
  }; // struct per_frame_data
  
  struct per_image_data {
    VkSemaphore render_finished_semaphore{nullptr};
  }; // struct per_image_data

  struct command_pool_key {
    queue::type type;
    std::thread::id thread_id;
  }; // struct command_pool_key

  struct command_pool_key_hash {
    auto operator()(const command_pool_key& key) const noexcept -> std::size_t {
      auto hash = std::size_t{0};
      utility::hash_combine(hash, key.type, key.thread_id);
      return hash;
    }
  }; // struct command_pool_key_hash

  struct command_pool_key_equality {
    auto operator()(const command_pool_key& lhs, const command_pool_key& rhs) const noexcept -> bool {
      return lhs.type == rhs.type && lhs.thread_id == rhs.thread_id;
    }
  }; // struct command_pool_key_equal

  auto _recreate_swapchain() -> void;

  auto _recreate_per_frame_data() -> void;

  auto _recreate_per_image_data() -> void;

  auto _recreate_command_buffers() -> void;

  graphics::instance _instance;
  graphics::physical_device _physical_device;
  graphics::logical_device _logical_device;
  graphics::allocator _allocator;
  graphics::surface _surface;

  std::unique_ptr<graphics::swapchain> _swapchain;

  std::unordered_map<command_pool_key, std::shared_ptr<graphics::command_pool>, command_pool_key_hash, command_pool_key_equality> _command_pools{};

  bool _is_framebuffer_resized;
  std::uint32_t _current_frame;

  std::array<per_frame_data, swapchain::max_frames_in_flight> _per_frame_data{};
  std::vector<per_image_data> _per_image_data{};

  std::vector<graphics::command_buffer> _graphics_command_buffers;
  std::vector<graphics::command_buffer> _compute_command_buffers;

}; // class graphics_module

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_
