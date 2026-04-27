// SPDX-License-Identifier: MIT
#ifndef LIBSBX_DEVICES_DEVICES_MODULE_HPP_
#define LIBSBX_DEVICES_DEVICES_MODULE_HPP_

#include <cinttypes>

#include <memory>
#include <vector>
#include <unordered_map>

#include <fmt/format.h>

#include <GLFW/glfw3.h>

#include <libsbx/core/module.hpp>
#include <libsbx/core/engine.hpp>

#include <libsbx/units/time.hpp>

#include <libsbx/memory/aligned_storage.hpp>

#include <libsbx/devices/window.hpp>
#include <libsbx/devices/input.hpp>

namespace sbx::devices {

class devices_module final : public core::module<devices_module> {

  inline static const auto is_registered = register_module(stage::pre);

  struct context {

    context() {
      if (!glfwInit()) {
        throw std::runtime_error{"Could not initialize glfw"};
      }

      if (!glfwVulkanSupported()) {
        throw std::runtime_error{"Glfw does not support vulkan"};
      }
    }

    ~context() {
      glfwTerminate();
    }

  }; // struct context

public:

  devices_module()
  : _context{},
    _window{window_create_info{"Demo", 1280, 720}} {
    const auto initial_size = math::vector2{_window.width(), _window.height()};

    input::set_active_viewport(math::vector2{0.0f, 0.0f}, initial_size);
    input::set_scene_input_active(true);

    _window.on_framebuffer_resized() += [](const auto& event) {
      input::set_active_viewport(math::vector2{0.0f, 0.0f}, math::vector2{static_cast<std::float_t>(event.width), static_cast<std::float_t>(event.height)});
    };
  }

  ~devices_module() override {

  }

  auto update() -> void override {
    input::_transition_pressed_keys();
    input::_transition_pressed_mouse_buttons();
    input::_transition_scroll_delta();
    
    glfwPollEvents();
  }

  auto window() -> devices::window& {
    return _window;
  }

  auto required_instance_extensions() const -> std::vector<const char*> {
    auto extension_count = std::uint32_t{0};
    auto** extensions = glfwGetRequiredInstanceExtensions(&extension_count);

    return std::vector<const char*>{extensions, extensions + extension_count};
  }

private:

  context _context;
  devices::window _window;

}; // class devices_module

} // namespace sbx::devices

#endif // LIBSBX_DEVICES_DEVICES_MODULE_HPP_
