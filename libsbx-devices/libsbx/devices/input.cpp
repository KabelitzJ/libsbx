#include <libsbx/devices/input.hpp>

#include <libsbx/devices/devices_module.hpp>

namespace sbx::devices {

core::delegate<math::vector2()> input::_mouse_position_callback;
std::unordered_map<key, key_state> input::_key_states;
std::unordered_map<mouse_button, key_state> input::_mouse_button_states;
math::vector2 input::_mouse_position;
math::vector2 input::_scroll_delta;

auto input::is_key_pressed(key key) -> bool {
  if (auto entry = _key_states.find(key); entry != _key_states.end()) {
    const auto& state = entry->second;

    return state.action == input_action::press;
  }

  return false;
}

auto input::is_key_down(key key) -> bool {
  if (auto entry = _key_states.find(key); entry != _key_states.end()) {
    const auto& state = entry->second;

    return state.action == input_action::press || state.action == input_action::repeat;
  }

  return false;
}

auto input::is_key_released(key key) -> bool {
  if (auto entry = _key_states.find(key); entry != _key_states.end()) {
    const auto& state = entry->second;

    return state.action == input_action::release;
  }

  return true;
}

auto input::is_mouse_button_pressed(mouse_button button) -> bool {
  if (auto entry = _mouse_button_states.find(button); entry != _mouse_button_states.end()) {
    const auto& state = entry->second;

    return state.action == input_action::press;
  }

  return false;
}

auto input::is_mouse_button_down(mouse_button button) -> bool {
  if (auto entry = _mouse_button_states.find(button); entry != _mouse_button_states.end()) {
    const auto& state = entry->second;

    return state.action == input_action::press || state.action == input_action::repeat;
  }

  return false;
}

auto input::is_mouse_button_released(mouse_button button) -> bool {
  if (auto entry = _mouse_button_states.find(button); entry != _mouse_button_states.end()) {
    const auto& state = entry->second;

    return state.action == input_action::release;
  }

  return true;
}

auto input::mouse_position() -> math::vector2 {
  return std::invoke(_mouse_position_callback);
}

auto input::scroll_delta() -> math::vector2 {
  return _scroll_delta;
}

auto input::_transition_pressed_keys() -> void {
  for (auto& [key, key_state] : _key_states) {
    if (key_state.action == input_action::press) {
      key_state.action = input_action::repeat;
    }
  }
}

auto input::_transition_pressed_mouse_buttons() -> void {
  for (auto& [button, key_state] : _mouse_button_states) {
    if (key_state.action == input_action::press) {
      key_state.action = input_action::repeat;
    }
  }
}

auto input::_transition_scroll_delta() -> void {
  _scroll_delta = math::vector2{};
}

auto input::_update_key_state(key key, input_action action) -> void {
  auto entry = _key_states.find(key);

  if (entry == _key_states.end()) {
    entry = _key_states.insert({key, key_state{input_action::release, input_action::release}}).first;
  }

  auto& state = entry->second;

  state.last_action = state.action;
  state.action = action;
}

auto input::_update_mouse_button_state(mouse_button button, input_action action) -> void {
  auto entry = _mouse_button_states.find(button);

  if (entry == _mouse_button_states.end()) {
    entry = _mouse_button_states.insert({button, key_state{input_action::release, input_action::release}}).first;
  }

  auto& state = entry->second;

  state.last_action = state.action;
  state.action = action;
}

auto input::_update_mouse_position(const math::vector2& position) -> void {
  _mouse_position = position;
}

auto input::_update_scroll_delta(const math::vector2& delta) -> void {
  _scroll_delta = delta;
}

} // namespace sbx::devices
