// SPDX-License-Identifier: MIT
#ifndef LIBSBX_PLATFORM_INPUT_HPP_
#define LIBSBX_PLATFORM_INPUT_HPP_

#include <cinttypes>
#include <unordered_map>

#include <libsbx/math/vector2.hpp>

#include <libsbx/platform/key.hpp>
#include <libsbx/platform/mouse_button.hpp>
#include <libsbx/platform/input_action.hpp>
#include <libsbx/platform/input_mod.hpp>

namespace sbx::platform {

struct key_state {
  input_action action;
  input_action last_action;
}; // struct key_state

class input {

  friend class platform_module;
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

  static auto scroll_delta() -> math::vector2;

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

  static math::vector2 _mouse_position;
  static math::vector2 _scroll_delta;

}; // class input

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_INPUT_HPP_
