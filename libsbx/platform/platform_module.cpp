// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/platform/platform_module.hpp>

#include <stdexcept>

#include <GLFW/glfw3.h>

#include <libsbx/utility/profiler.hpp>

namespace sbx::platform {

platform_module::context::context() {
  if (!glfwInit()) {
    throw std::runtime_error{"Could not initialize glfw"};
  }

  if (!glfwVulkanSupported()) {
    throw std::runtime_error{"Glfw does not support vulkan"};
  }
}

platform_module::context::~context() {
  glfwTerminate();
}

platform_module::platform_module()
: _context{},
  _window{window_create_info{"libsbx", 1280u, 720u}} { }

platform_module::~platform_module() {

}

auto platform_module::pre_update() -> void {
  SBX_PROFILE_SCOPE("platform_module::update");

  input::_transition_pressed_keys();
  input::_transition_pressed_mouse_buttons();
  input::_transition_scroll_delta();

  glfwPollEvents();
}

auto platform_module::window() -> platform::window& {
  return _window;
}

auto platform_module::required_instance_extensions() const -> std::vector<const char*> {
  auto extension_count = std::uint32_t{0};
  auto** extensions = glfwGetRequiredInstanceExtensions(&extension_count);

  return std::vector<const char*>{extensions, extensions + extension_count};
}

} // namespace sbx::platform
