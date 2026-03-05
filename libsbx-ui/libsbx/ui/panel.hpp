// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_PANEL_HPP_
#define LIBSBX_UI_PANEL_HPP_

#include <libsbx/ui/element.hpp>

namespace sbx::ui {

class panel : public element {

public:

  panel() = default;

  ~panel() override = default;

protected:

  auto submit(sprites::sprites_module& sprites, const math::vector2& screen_size) -> void override {
    const auto albedo_index = sprites.register_image({});

    const auto& r = computed_rect();

    const auto pivot_x = r.x + r.width * pivot.x();
    const auto pivot_y = r.y + r.height * pivot.y();

    submit_quad(sprites, screen_size, {pivot_x, pivot_y}, {r.width, r.height}, pivot, color, albedo_index, sort_order, {0.0f, 0.0f}, {1.0f, 1.0f}, 0u);
  }

}; // class panel

} // namespace sbx::ui

#endif // LIBSBX_UI_PANEL_HPP_
