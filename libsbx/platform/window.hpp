// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PLATFORM_WINDOW_HPP_
#define LIBSBX_PLATFORM_WINDOW_HPP_

#include <cstdint>
#include <variant>
#include <string>
#include <functional>
#include <memory>

namespace sbx::platform {

struct win32_window_info {
  void* hwnd{nullptr};
  void* hinstance{nullptr};
}; // struct win32_window_info

struct x11_window_info {
  void* display{nullptr};
  std::uint64_t window{0};
}; // struct x11_window_info

struct wayland_window_info {
  void* display{nullptr};
  void* surface{nullptr};
}; // struct wayland_window_info

using native_window_info = std::variant<win32_window_info, x11_window_info, wayland_window_info>;

struct window_description {
  std::uint32_t width{1280};
  std::uint32_t height{720};
  std::string title{"norn"};
  bool is_resizable{true};
}; // struct window_description

class window {

public:

  window(const window_description& description)
  : _description{description} { }

  virtual ~window() = default;

  static auto create(const window_description& description) -> std::unique_ptr<window>;

  virtual auto poll_events() -> void = 0;
  virtual auto should_close() const -> bool = 0;
  virtual auto get_native_info() const -> native_window_info = 0;

protected:

  window_description _description;

}; // class window

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_WINDOW_HPP_