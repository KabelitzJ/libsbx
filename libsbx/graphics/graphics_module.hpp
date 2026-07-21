// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_
#define LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_

#include <memory>

#include <libsbx/utility/hash.hpp>
#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/platform/platform_module.hpp>

#include <libsbx/graphics/frame_context.hpp>

#include <libsbx/graphics/commands/command_pool.hpp>

#include <libsbx/graphics/devices/instance.hpp>
#include <libsbx/graphics/devices/physical_device.hpp>
#include <libsbx/graphics/devices/logical_device.hpp>
#include <libsbx/graphics/devices/allocator.hpp>
#include <libsbx/graphics/devices/surface.hpp>

#include <libsbx/graphics/resources/resource_registry.hpp>

namespace sbx::graphics {

/**
 * @brief Owns the device and everything that lives for the whole run.
 *
 * The module participates in no engine stage. It holds the device objects, the resource pools and
 * the frame context, and hands them out. Driving the frame is the render module's job, which calls
 * `frame().begin_frame()` and `frame().end_frame()` during the render stage.
 */
class graphics_module final : public utility::noncopyable {

public:

  using dependencies = core::dependency_list<platform::platform_module>;

  graphics_module();

  ~graphics_module();

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

  /**
   * @brief The frame loop: timeline, swapchain, per frame command buffers.
   */
  [[nodiscard]] auto frame_context() noexcept -> graphics::frame_context& {
    return _frame_context;
  }

  /**
   * @brief The resource pools. Retire with `frame().frame_index()`; collection happens in
   * `frame_context::begin_frame`.
   */
  [[nodiscard]] auto resource_registry() noexcept -> graphics::resource_registry& {
    return _resource_registry;
  }

  auto command_pool(const queue::type type, const std::thread::id& thread_id = std::this_thread::get_id()) -> const std::shared_ptr<command_pool>&;

private:

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
  }; // struct command_pool_key_equality

  graphics::instance _instance;
  graphics::physical_device _physical_device;
  graphics::logical_device _logical_device;
  graphics::allocator _allocator;
  graphics::surface _surface;

  std::unordered_map<command_pool_key, std::shared_ptr<graphics::command_pool>, command_pool_key_hash, command_pool_key_equality> _command_pools{};

  graphics::resource_registry _resource_registry{};
  graphics::frame_context _frame_context{};

}; // class graphics_module

} // namespace sbx::graphics

#endif // LIBSBX_GRAPHICS_GRAPHICS_MODULE_HPP_
