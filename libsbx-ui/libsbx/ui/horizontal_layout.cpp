// SPDX-License-Identifier: MIT
#include <libsbx/ui/horizontal_layout.hpp>

namespace sbx::ui {

horizontal_layout::horizontal_layout(std::float_t spacing)
: spacing{spacing} { }

auto horizontal_layout::arrange(const rectangle& bounds, std::vector<std::unique_ptr<element>>& children) -> void {
  const auto content = content_rectangle(bounds);

  if (children.empty()) {
    return;
  }

  auto total_fixed = 0.0f;
  auto total_flex = 0.0f;

  for (auto& child : children) {
    if (child->sizing.flex > 0.0f) {
      total_flex += child->sizing.flex;
    } else {
      total_fixed += child->sizing.preferred.x();
    }
  }

  const auto total_spacing = spacing * static_cast<std::float_t>(children.size() - 1u);
  const auto remaining = std::max(0.0f, content.width - total_fixed - total_spacing);

  auto cursor_x = content.x;

  for (auto& child : children) {
    auto child_width = 0.0f;

    if (child->sizing.flex > 0.0f) {
      child_width = total_flex > 0.0f ? (child->sizing.flex / total_flex) * remaining : 0.0f;
    } else {
      child_width = child->sizing.preferred.x();
    }

    child_width = std::clamp(child_width, child->sizing.min.x(), child->sizing.max.x());

    auto child_height = child->sizing.preferred.y() > 0.0f ? std::clamp(child->sizing.preferred.y(), child->sizing.min.y(), child->sizing.max.y()) : content.height;

    child->resolve_as_arranged(rectangle{cursor_x, content.y, child_width, child_height});

    cursor_x += child_width + spacing;
  }
}

} // namespace sbx::ui