// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef LIBSBX_PLATFORM_INPUT_HPP_
#define LIBSBX_PLATFORM_INPUT_HPP_

#include <cinttypes>
#include <array>

#include <libsbx/math/vector2.hpp>

#include <libsbx/platform/key.hpp>
#include <libsbx/platform/mouse_button.hpp>
#include <libsbx/platform/input_action.hpp>
#include <libsbx/platform/input_mod.hpp>

namespace sbx::platform {

struct key_state {
  // action is glfw's raw last-reported level (press or release; a key/button never actually gets
  // written to repeat here -- see window.cpp's callbacks). last_action is that same value as of the
  // start of the previous frame (see input::_snapshot_key_states/_snapshot_mouse_button_states),
  // letting is_key_pressed/is_key_released edge-detect by comparing the two instead of needing a
  // decaying state machine of their own.
  input_action action{input_action::release};
  input_action last_action{input_action::release};
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

  static auto _snapshot_key_states() -> void;
  static auto _snapshot_mouse_button_states() -> void;
  static auto _transition_scroll_delta() -> void;

  static auto _update_key_state(key key, input_action action) -> void;
  static auto _update_mouse_button_state(mouse_button button, input_action action) -> void;
  static auto _update_mouse_position(const math::vector2& position) -> void;
  static auto _update_scroll_delta(const math::vector2& delta) -> void;

  // key's range is [unknown(-1), menu(348)] -- offset by one so unknown lands on slot 0 instead of
  // needing a separate bounds/sentinel check on every lookup.
  inline static constexpr auto key_count = std::size_t{350u};
  inline static constexpr auto mouse_button_count = std::size_t{8u};

  [[nodiscard]] static auto _key_index(key value) noexcept -> std::size_t {
    return static_cast<std::size_t>(static_cast<std::int32_t>(value) + 1);
  }

  [[nodiscard]] static auto _mouse_button_index(mouse_button value) noexcept -> std::size_t {
    return static_cast<std::size_t>(value);
  }

  [[nodiscard]] static constexpr auto _is_down(input_action action) noexcept -> bool {
    return action == input_action::press || action == input_action::repeat;
  }

  static std::array<key_state, key_count> _key_states;
  static std::array<key_state, mouse_button_count> _mouse_button_states;

  static math::vector2 _mouse_position;
  static math::vector2 _scroll_delta;

}; // class input

} // namespace sbx::platform

#endif // LIBSBX_PLATFORM_INPUT_HPP_
