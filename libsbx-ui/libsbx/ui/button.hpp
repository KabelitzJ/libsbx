// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_BUTTON_HPP_
#define LIBSBX_UI_BUTTON_HPP_

#include <functional>

#include <libsbx/math/color.hpp>

#include <libsbx/ui/panel.hpp>

namespace sbx::ui {

class button : public panel {

public:

  math::color normal_color{0.3f, 0.3f, 0.3f, 1.0f};
  math::color hovered_color{0.4f, 0.4f, 0.4f, 1.0f};
  math::color pressed_color{0.2f, 0.2f, 0.2f, 1.0f};

  std::function<void()> on_click{};

  button() {
    is_interactive = true;
    color = normal_color;
  }

  ~button() override = default;

protected:

  auto process_input(const math::vector2& mouse_position, bool is_down, bool was_down) -> bool override {
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

}; // class button

} // namespace sbx::ui

#endif // LIBSBX_UI_BUTTON_HPP_
