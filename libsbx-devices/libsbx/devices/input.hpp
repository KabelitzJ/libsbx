// SPDX-License-Identifier: MIT
#ifndef LIBSBX_DEVICES_INPUT_HPP_
#define LIBSBX_DEVICES_INPUT_HPP_

#include <cinttypes>
#include <unordered_map>

#include <GLFW/glfw3.h>

#include <libsbx/utility/bitmask.hpp>

#include <libsbx/math/vector2.hpp>

#include <libsbx/devices/key.hpp>
#include <libsbx/devices/mouse_button.hpp>
#include <libsbx/devices/input_action.hpp>
#include <libsbx/devices/input_mod.hpp>

namespace sbx::devices {

struct key_state {
  input_action action;
  input_action last_action;
}; // struct key_state

class input {

  friend class devices_module;
  friend class window;

public:

  input() = delete;

  static auto is_key_pressed(key key) -> bool;
  static auto is_key_down(key key) -> bool;
  static auto is_key_released(key key) -> bool;

  static auto is_mouse_button_pressed(mouse_button button) -> bool;
  static auto is_mouse_button_down(mouse_button button) -> bool;
  static auto is_mouse_button_released(mouse_button button) -> bool;

  static auto mouse_position() -> math::vector2;

  static auto mouse_window_position() -> math::vector2;

  static auto scroll_delta() -> math::vector2;

  static auto set_active_viewport(const math::vector2& origin, const math::vector2& size) -> void;

  static auto active_viewport_origin() -> math::vector2;

  static auto active_viewport_size() -> math::vector2;

private:

  static auto _transition_pressed_keys() -> void;
  static auto _transition_pressed_mouse_buttons() -> void;
  static auto _transition_scroll_delta() -> void;

  static auto _update_key_state(key key, input_action action) -> void;
  static auto _update_mouse_button_state(mouse_button button, input_action action) -> void;
  static auto _update_mouse_position(const math::vector2& position) -> void;
  static auto _update_scroll_delta(const math::vector2& delta) -> void;

  static std::unordered_map<key, key_state> _key_states;
  static std::unordered_map<mouse_button, key_state> _mouse_button_states;

  static math::vector2 _mouse_window_position;
  static math::vector2 _scroll_delta;

  static math::vector2 _active_viewport_origin;
  static math::vector2 _active_viewport_size;

}; // class input

} // namespace sbx::devices

#endif // LIBSBX_DEVICES_INPUT_HPP_
