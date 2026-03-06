// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_VERTICAL_LAYOUT_HPP_
#define LIBSBX_UI_VERTICAL_LAYOUT_HPP_

#include <algorithm>

#include <libsbx/ui/layout.hpp>
#include <libsbx/ui/element.hpp>

namespace sbx::ui {

class vertical_layout : public layout {

public:

  std::float_t spacing{0.0f};

  vertical_layout() = default;

  vertical_layout(std::float_t spacing)
  : spacing{spacing} { }

  ~vertical_layout() override = default;

  auto arrange(const rectangle& bounds, std::vector<std::unique_ptr<element>>& children) -> void override {
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
        total_fixed += child->sizing.preferred.y();
      }
    }

    const auto total_spacing = spacing * static_cast<std::float_t>(children.size() - 1u);
    const auto remaining = std::max(0.0f, content.height - total_fixed - total_spacing);

    auto cursor_y = content.y + content.height;

    for (auto& child : children) {
      auto child_height = 0.0f;

      if (child->sizing.flex > 0.0f) {
        child_height = total_flex > 0.0f ? (child->sizing.flex / total_flex) * remaining : 0.0f;
      } else {
        child_height = child->sizing.preferred.y();
      }

      child_height = std::clamp(child_height, child->sizing.min.y(), child->sizing.max.y());

      auto child_width = child->sizing.preferred.x() > 0.0f ? std::clamp(child->sizing.preferred.x(), child->sizing.min.x(), child->sizing.max.x()) : content.width;

      cursor_y -= child_height;

      child->resolve_as_arranged(rectangle{content.x, cursor_y, child_width, child_height});

      cursor_y -= spacing;
    }
  }

}; // class vertical_layout

} // namespace sbx::ui

#endif // LIBSBX_UI_VERTICAL_LAYOUT_HPP_
