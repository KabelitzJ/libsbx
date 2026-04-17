// SPDX-License-Identifier: MIT
#include <libsbx/ui/button.hpp>

namespace sbx::ui {

button::button() {
  is_interactive = true;
  color = normal_color;
}

auto button::process_input(const math::vector2& mouse_position, bool is_down, bool was_down) -> bool {
  const auto hit = computed_rectangle().contains(mouse_position);

  if (hit) {
    if (is_down) {
      color = pressed_color;
    } else {
      color = hovered_color;
    }

    if (was_down && !is_down && on_click) {
      on_click();
    }

    return true;
  }

  color = normal_color;

  return false;
}

} // namespace sbx::ui