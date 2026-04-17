// SPDX-License-Identifier: MIT
#include <libsbx/ui/layout.hpp>

namespace sbx::ui {

[[nodiscard]] auto rectangle::contains(const math::vector2& point) const -> bool {
  return point.x() >= x && point.x() <= x + width && point.y() >= y && point.y() <= y + height;
}

[[nodiscard]] auto layout::content_rectangle(const rectangle& bounds) const -> rectangle {
  return rectangle{
    bounds.x + padding.left,
    bounds.y + padding.bottom,
    bounds.width - padding.left - padding.right,
    bounds.height - padding.bottom - padding.top
  };
}

} // namespace sbx::ui