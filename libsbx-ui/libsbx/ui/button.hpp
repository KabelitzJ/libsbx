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

  button();

  ~button() override = default;

protected:

  auto process_input(const math::vector2& mouse_position, bool is_down, bool was_down) -> bool override;

}; // class button

} // namespace sbx::ui

#endif // LIBSBX_UI_BUTTON_HPP_
