// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PLATFORM_EVENTS_HPP_
#define LIBSBX_PLATFORM_EVENTS_HPP_

#include <cinttypes>
#include <cmath>

#include <libsbx/platform/key.hpp>
#include <libsbx/platform/mouse_button.hpp>
#include <libsbx/platform/input_action.hpp>
#include <libsbx/platform/input_mod.hpp>

namespace sbx::platform {

/** 
 * @brief An event that is fired when a window requests to be closed 
 */
struct window_closed_event { };

struct window_moved_event {
  std::int32_t x{};
  std::int32_t y{};
};

struct window_resized_event {
  std::int32_t width{};
  std::int32_t height{};
};

struct framebuffer_resized_event {
  std::int32_t width{};
  std::int32_t height{};
};

struct key_pressed_event {
  platform::key key;
  input_mod mods;
}; // struct key_pressed_event

struct key_released_event {
  platform::key key;
  input_mod mods;
}; // struct key_released_event

struct mouse_moved_event {
  std::float_t x;
  std::float_t y;
}; // struct mouse_moved_event

struct mouse_button_pressed_event {
  mouse_button button;
  input_mod mods;
}; // struct mouse_button_pressed_event

struct mouse_button_released_event {
  mouse_button button;
  input_mod mods;
}; // struct mouse_button_released_event

struct mouse_scrolled_event {
  std::float_t x;
  std::float_t y;
}; // struct mouse_scrolled_event

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_EVENTS_HPP_
