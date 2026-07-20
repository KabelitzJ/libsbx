// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PLATFORM_WINDOW_HPP_
#define LIBSBX_PLATFORM_WINDOW_HPP_

#include <cmath>
#include <cstdint>
#include <string>
#include <variant>

#include <GLFW/glfw3.h>

#include <libsbx/utility/noncopyable.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/signals/signal.hpp>

#include <libsbx/platform/events.hpp>

namespace sbx::platform {

struct window_create_info {
  std::string title{};
  std::uint32_t width{};
  std::uint32_t height{};
}; // struct window_create_info

class window : public utility::noncopyable {

public:

  using handle_type = GLFWwindow*;

  window(const window_create_info& create_info);

  ~window();

  auto handle() const noexcept -> handle_type;

  operator handle_type() const noexcept;

  auto title() const -> const std::string&;

  auto set_title(const std::string& title) -> void;

  auto width() const -> std::uint32_t;

  auto height() const -> std::uint32_t;

  auto aspect_ratio() const -> std::float_t;

  auto should_close() -> bool;

  auto show() -> void;

  auto hide() -> void;

  auto is_iconified() const noexcept -> bool;

  auto is_focused() const noexcept -> bool;

  auto is_visible() const noexcept -> bool;

  auto on_window_closed() -> signals::signal<const window_closed_event&>&;

  auto on_window_moved() -> signals::signal<const window_moved_event&>&;

  auto on_window_resized() -> signals::signal<const window_resized_event&>&;

  auto on_framebuffer_resized() -> signals::signal<const framebuffer_resized_event&>&;

  auto on_key_pressed() -> signals::signal<const key_pressed_event&>&;

  auto on_key_released() -> signals::signal<const key_released_event&>&;

  auto on_mouse_moved() -> signals::signal<const mouse_moved_event&>&;

  auto on_mouse_button_pressed() -> signals::signal<const mouse_button_pressed_event&>&;

  auto on_mouse_button_released() -> signals::signal<const mouse_button_released_event&>&;

  auto on_mouse_scrolled() -> signals::signal<const mouse_scrolled_event&>&;

private:

  auto _set_callbacks() -> void;

  std::string _title{};
  std::uint32_t _width{};
  std::uint32_t _height{};

  handle_type _handle{};

  math::vector2 _last_mouse_position;

  signals::signal<const window_closed_event&> _on_window_closed;
  signals::signal<const window_moved_event&> _on_window_moved;
  signals::signal<const window_resized_event&> _on_window_resized;
  signals::signal<const framebuffer_resized_event&> _on_framebuffer_resized;
  signals::signal<const key_pressed_event&> _on_key_pressed;
  signals::signal<const key_released_event&> _on_key_released;
  signals::signal<const mouse_moved_event&> _on_mouse_moved;
  signals::signal<const mouse_button_pressed_event&> _on_mouse_button_pressed;
  signals::signal<const mouse_button_released_event&> _on_mouse_button_released;
  signals::signal<const mouse_scrolled_event&> _on_mouse_scrolled;

}; // class window

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_WINDOW_HPP_
