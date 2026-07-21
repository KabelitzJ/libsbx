// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PLATFORM_PLATFORM_MODULE_HPP_
#define LIBSBX_PLATFORM_PLATFORM_MODULE_HPP_

#include <vector>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/core/module.hpp>

#include <libsbx/platform/window.hpp>
#include <libsbx/platform/input.hpp>

namespace sbx::platform {

class platform_module : public utility::noncopyable {

  /**
   * @brief Owns the glfw library lifetime. Must be initialized before and
   * destroyed after the window.
   */
  struct context {

    context();

    ~context();

  }; // struct context

public:

  platform_module();

  ~platform_module();

  auto pre_update() -> void;

  auto window() -> platform::window&;

  /**
   * @brief Vulkan instance extensions the platform needs for surface creation.
   */
  auto required_instance_extensions() const -> std::vector<const char*>;

private:

  context _context;
  platform::window _window;

}; // class platform_module

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_PLATFORM_MODULE_HPP_
