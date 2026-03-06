// SPDX-License-Identifier: MIT
#include <libsbx/ui/panel.hpp>

namespace sbx::ui {

auto panel::submit(const math::vector2& screen_size) -> void {
  const auto& r = computed_rectangle();

  const auto pivot_x = r.x + r.width * pivot.x();
  const auto pivot_y = r.y + r.height * pivot.y();

  submit_quad(screen_size, {pivot_x, pivot_y}, {r.width, r.height}, pivot, color, graphics::separate_image2d_array::max_size, sort_order, {0.0f, 0.0f}, {1.0f, 1.0f}, 0u);
}

} // namespace sbx::ui