// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/platform/window.hpp>

#include <stdexcept>

#include <fmt/format.h>

#include <libsbx/version.hpp>

#include <libsbx/utility/overload.hpp>
#include <libsbx/utility/target.hpp>

#include <libsbx/platform/input.hpp>

namespace sbx::platform {

window::window(const create_info& create_info)
: _last_mouse_position{-1.0f, -1.0f} {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  std::visit([this](const auto& info) {
    _title = info.title;
  }, create_info);

  if constexpr (utility::build_type_v == utility::build_type::debug) {
    _title = fmt::format("{} [Debug] v" SBX_VERSION_STRING, _title);
  }

  auto* monitor = glfwGetPrimaryMonitor();

  if (!monitor) {
    throw std::runtime_error{"Could not get primary monitor"};
  }

  const auto* video_mode = glfwGetVideoMode(monitor);

  if (!video_mode) {
    throw std::runtime_error{"Could not get video mode"};
  }

  std::visit(utility::overload(
    [this](const windowed_create_info& info) {
      _width = info.size.x();
      _height = info.size.y();

      _handle = glfwCreateWindow(static_cast<std::int32_t>(_width), static_cast<std::int32_t>(_height), _title.c_str(), nullptr, nullptr);
    },
    [this, monitor, video_mode](const fullscreen_create_info& info) {
      _width = info.size.x();
      _height = info.size.y();

      _handle = glfwCreateWindow(static_cast<std::int32_t>(_width), static_cast<std::int32_t>(_height), _title.c_str(), monitor, nullptr);
    },
    [this, monitor, video_mode]([[maybe_unused]] const borderless_create_info& info) {
      _width = static_cast<std::uint32_t>(video_mode->width);
      _height = static_cast<std::uint32_t>(video_mode->height);

      glfwWindowHint(GLFW_RED_BITS, video_mode->redBits);
      glfwWindowHint(GLFW_GREEN_BITS, video_mode->greenBits);
      glfwWindowHint(GLFW_BLUE_BITS, video_mode->blueBits);
      glfwWindowHint(GLFW_REFRESH_RATE, video_mode->refreshRate);

      _handle = glfwCreateWindow(static_cast<std::int32_t>(_width), static_cast<std::int32_t>(_height), _title.c_str(), monitor, nullptr);
    }
  ), create_info);

  if (!_handle) {
    throw std::runtime_error{"Could not create glfw window"};
  }
  
  if (std::holds_alternative<windowed_create_info>(create_info)) {
    glfwSetWindowPos(_handle, (video_mode->width - static_cast<std::int32_t>(_width)) / 2, (video_mode->height - static_cast<std::int32_t>(_height)) / 2);
  }
    
  glfwFocusWindow(_handle);

  if (glfwRawMouseMotionSupported()) {
    glfwSetInputMode(_handle, GLFW_RAW_MOUSE_MOTION, true);
  }

  glfwSetInputMode(_handle, GLFW_STICKY_KEYS, true);
  glfwSetInputMode(_handle, GLFW_LOCK_KEY_MODS, true);

  _set_callbacks();
}

window::~window() {
  glfwDestroyWindow(_handle);
}

auto window::handle() const noexcept -> handle_type {
  return _handle;
}

window::operator handle_type() const noexcept {
  return _handle;
}

auto window::title() const -> const std::string& {
  return _title;
}

auto window::set_title(const std::string& title) -> void {
  _title = title;

  if constexpr (utility::build_type_v == utility::build_type::debug) {
    _title = fmt::format("{} [Debug] v" SBX_VERSION_STRING, _title);
  }

  glfwSetWindowTitle(_handle, _title.c_str());
}

auto window::width() const -> std::uint32_t {
  return _width;
}

auto window::height() const -> std::uint32_t {
  return _height;
}

auto window::aspect_ratio() const -> std::float_t {
  return static_cast<std::float_t>(_width) / static_cast<std::float_t>(_height);
}

auto window::should_close() -> bool {
  return glfwWindowShouldClose(_handle);
}

auto window::show() -> void {
  glfwShowWindow(_handle);
}

auto window::hide() -> void {
  glfwHideWindow(_handle);
}

auto window::is_iconified() const noexcept -> bool {
  return glfwGetWindowAttrib(_handle, GLFW_ICONIFIED);
}

auto window::is_focused() const noexcept -> bool {
  return glfwGetWindowAttrib(_handle, GLFW_FOCUSED);
}

auto window::is_visible() const noexcept -> bool {
  return glfwGetWindowAttrib(_handle, GLFW_VISIBLE);
}

auto window::on_window_closed() -> signals::signal<const window_closed_event&>& {
  return _on_window_closed;
}

auto window::on_window_moved() -> signals::signal<const window_moved_event&>& {
  return _on_window_moved;
}

auto window::on_window_resized() -> signals::signal<const window_resized_event&>& {
  return _on_window_resized;
}

auto window::on_framebuffer_resized() -> signals::signal<const framebuffer_resized_event&>& {
  return _on_framebuffer_resized;
}

auto window::on_key_pressed() -> signals::signal<const key_pressed_event&>& {
  return _on_key_pressed;
}

auto window::on_key_released() -> signals::signal<const key_released_event&>& {
  return _on_key_released;
}

auto window::on_mouse_moved() -> signals::signal<const mouse_moved_event&>& {
  return _on_mouse_moved;
}

auto window::on_mouse_button_pressed() -> signals::signal<const mouse_button_pressed_event&>& {
  return _on_mouse_button_pressed;
}

auto window::on_mouse_button_released() -> signals::signal<const mouse_button_released_event&>& {
  return _on_mouse_button_released;
}

auto window::on_mouse_scrolled() -> signals::signal<const mouse_scrolled_event&>& {
  return _on_mouse_scrolled;
}

auto window::_set_callbacks() -> void {
  glfwSetWindowUserPointer(_handle, this);

  glfwSetWindowCloseCallback(_handle, [](GLFWwindow* window){
    auto& self = *static_cast<platform::window*>(glfwGetWindowUserPointer(window));

    self._on_window_closed(window_closed_event{});
  });

  glfwSetWindowPosCallback(_handle, [](GLFWwindow* window, std::int32_t x, std::int32_t y){
    auto& self = *static_cast<platform::window*>(glfwGetWindowUserPointer(window));

    self._on_window_moved(window_moved_event{x, y});
  });

  glfwSetWindowSizeCallback(_handle, [](GLFWwindow* window, std::int32_t width, std::int32_t height){
    auto& self = *static_cast<platform::window*>(glfwGetWindowUserPointer(window));

    self._on_window_resized(window_resized_event{width, height});
  });

  glfwSetFramebufferSizeCallback(_handle, [](GLFWwindow* window, std::int32_t width, std::int32_t height){
    auto& self = *static_cast<platform::window*>(glfwGetWindowUserPointer(window));

    self._on_framebuffer_resized(framebuffer_resized_event{width, height});

    self._width = static_cast<std::uint32_t>(width);
    self._height = static_cast<std::uint32_t>(height);
  });

  glfwSetKeyCallback(_handle, [](GLFWwindow* window, std::int32_t key, [[maybe_unused]] std::int32_t scancode, std::int32_t action, std::int32_t mods){
    auto& self = *static_cast<platform::window*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS) {
      input::_update_key_state(static_cast<platform::key>(key), input_action::press);
      self._on_key_pressed(key_pressed_event{static_cast<platform::key>(key), static_cast<platform::input_mod>(mods)});
    } else if (action == GLFW_RELEASE) {
      input::_update_key_state(static_cast<platform::key>(key), input_action::release);
      self._on_key_released(key_released_event{static_cast<platform::key>(key), static_cast<platform::input_mod>(mods)});
    }
  });

  glfwSetCursorPosCallback(_handle, [](GLFWwindow* window, double x, double y){
    auto& self = *static_cast<platform::window*>(glfwGetWindowUserPointer(window));

    const auto mouse_position = math::vector2{static_cast<std::float_t>(x), static_cast<std::float_t>(y)};

    if (self._last_mouse_position.x() < 0.0f || self._last_mouse_position.y() < 0.0f) {
      self._on_mouse_moved(mouse_moved_event{mouse_position.x(), mouse_position.y()});
    } else {
      self._on_mouse_moved(mouse_moved_event{mouse_position.x() - self._last_mouse_position.x(), mouse_position.y() - self._last_mouse_position.y()});
    }

    self._last_mouse_position = mouse_position;

    input::_update_mouse_position(mouse_position);
  });

  glfwSetMouseButtonCallback(_handle, [](GLFWwindow* window, std::int32_t button, std::int32_t action, std::int32_t mods){
    auto& self = *static_cast<platform::window*>(glfwGetWindowUserPointer(window));

    if (action == GLFW_PRESS) {
      input::_update_mouse_button_state(static_cast<platform::mouse_button>(button), input_action::press);
      self._on_mouse_button_pressed(mouse_button_pressed_event{static_cast<platform::mouse_button>(button), static_cast<platform::input_mod>(mods)});
    } else if (action == GLFW_RELEASE) {
      input::_update_mouse_button_state(static_cast<platform::mouse_button>(button), input_action::release);
      self._on_mouse_button_released(mouse_button_released_event{static_cast<platform::mouse_button>(button), static_cast<platform::input_mod>(mods)});
    }
  });

  glfwSetScrollCallback(_handle, [](GLFWwindow* window, double x, double y){
    auto& self = *static_cast<platform::window*>(glfwGetWindowUserPointer(window));

    self._on_mouse_scrolled(mouse_scrolled_event{static_cast<std::float_t>(x), static_cast<std::float_t>(y)});

    input::_update_scroll_delta(math::vector2{static_cast<std::float_t>(x), static_cast<std::float_t>(y)});
  });
}

} // namespace sbx::platform
