// SPDX-License-Identifier: MIT
#ifndef LIBSBX_DEVICES_WINDOW_HPP_
#define LIBSBX_DEVICES_WINDOW_HPP_

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <functional>
#include <filesystem>
#include <unordered_set>
#include <cmath>

#include <libsbx/utility/target.hpp>

#include <libsbx/core/concepts.hpp>
#include <libsbx/core/delegate.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/signals/signal.hpp>

#include <libsbx/devices/events.hpp>
#include <libsbx/devices/input.hpp>

namespace sbx::devices {

struct window_create_info {
  std::string title{};
  std::uint32_t width{};
  std::uint32_t height{};
}; // struct window_create_info

class window {

public:

  window(const window_create_info& create_info);

  ~window();

  auto handle() -> GLFWwindow*;

  operator GLFWwindow*();

  auto title() const -> const std::string&;

  auto set_title(const std::string& title) -> void;

  auto width() const -> std::uint32_t;

  auto height() const -> std::uint32_t;

  auto aspect_ratio() const -> std::float_t;

  /**
   * @brief Determins if the window should be closed
   * @return true if the window should be closed, false otherwise
   */
  auto should_close() -> bool;

  /**
   * @brief Makes the window visible
   */
  auto show() -> void;

  /**
   * @brief Hides the window
   */
  auto hide() -> void;

  auto is_iconified() const noexcept -> bool;

  auto is_focused() const noexcept -> bool;

  auto is_visible() const noexcept -> bool;

  auto on_window_closed_signal() -> signals::signal<const window_closed_event&>&;

  auto on_window_moved_signal() -> signals::signal<const window_moved_event&>&;

  auto on_window_resized_signal() -> signals::signal<const window_resized_event&>&;

  auto on_framebuffer_resized() -> signals::signal<const framebuffer_resized_event&>&;

  auto on_key_pressed() -> signals::signal<const key_pressed_event&>&;

  auto on_key_released() -> signals::signal<const key_released_event&>&;

  auto on_mouse_moved() -> signals::signal<const mouse_moved_event&>&;

  auto mouse_position() const -> const math::vector2& {
    return _last_mouse_position;
  }

private:

  auto _set_callbacks() -> void;

  std::string _title{};
  std::uint32_t _width{};
  std::uint32_t _height{};

  GLFWwindow* _handle{};

  math::vector2 _last_mouse_position;

  signals::signal<const window_closed_event&> _on_window_closed_signal;
  signals::signal<const window_moved_event&> _on_window_moved_signal;
  signals::signal<const window_resized_event&> _on_window_resized_signal;
  signals::signal<const framebuffer_resized_event&> _on_framebuffer_resized;
  signals::signal<const key_pressed_event&> _on_key_pressed;
  signals::signal<const key_released_event&> _on_key_released;
  signals::signal<const mouse_moved_event&> _on_mouse_moved;
  signals::signal<const mouse_button_pressed_event&> _on_mouse_button_pressed;
  signals::signal<const mouse_button_released_event&> _on_mouse_button_released;
  signals::signal<const mouse_scrolled_event&> _on_mouse_scrolled;

}; // class window

} // namespace sbx::devices

#endif // LIBSBX_DEVICES_WINDOW_HPP_
