// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#include <libsbx/platform/input.hpp>

namespace sbx::platform {

std::array<key_state, input::key_count> input::_key_states{};
std::array<key_state, input::mouse_button_count> input::_mouse_button_states{};
math::vector2 input::_mouse_position;
math::vector2 input::_scroll_delta;

auto input::is_key_pressed(key key) -> bool {
  const auto& state = _key_states[_key_index(key)];

  return _is_down(state.action) && !_is_down(state.last_action);
}

auto input::is_key_down(key key) -> bool {
  return _is_down(_key_states[_key_index(key)].action);
}

auto input::is_key_released(key key) -> bool {
  const auto& state = _key_states[_key_index(key)];

  return !_is_down(state.action) && _is_down(state.last_action);
}

auto input::is_mouse_button_pressed(mouse_button button) -> bool {
  const auto& state = _mouse_button_states[_mouse_button_index(button)];

  return _is_down(state.action) && !_is_down(state.last_action);
}

auto input::is_mouse_button_down(mouse_button button) -> bool {
  return _is_down(_mouse_button_states[_mouse_button_index(button)].action);
}

auto input::is_mouse_button_released(mouse_button button) -> bool {
  const auto& state = _mouse_button_states[_mouse_button_index(button)];

  return !_is_down(state.action) && _is_down(state.last_action);
}

auto input::mouse_position() -> math::vector2 {
  return _mouse_position;
}

auto input::scroll_delta() -> math::vector2 {
  return _scroll_delta;
}

auto input::_snapshot_key_states() -> void {
  for (auto& state : _key_states) {
    state.last_action = state.action;
  }
}

auto input::_snapshot_mouse_button_states() -> void {
  for (auto& state : _mouse_button_states) {
    state.last_action = state.action;
  }
}

auto input::_transition_scroll_delta() -> void {
  _scroll_delta = math::vector2{};
}

auto input::_update_key_state(key key, input_action action) -> void {
  _key_states[_key_index(key)].action = action;
}

auto input::_update_mouse_button_state(mouse_button button, input_action action) -> void {
  _mouse_button_states[_mouse_button_index(button)].action = action;
}

auto input::_update_mouse_position(const math::vector2& position) -> void {
  _mouse_position = position;
}

auto input::_update_scroll_delta(const math::vector2& delta) -> void {
  _scroll_delta = delta;
}

} // namespace sbx::platform
