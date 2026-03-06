// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_GRID_LAYOUT_HPP_
#define LIBSBX_UI_GRID_LAYOUT_HPP_

#include <algorithm>

#include <libsbx/ui/layout.hpp>
#include <libsbx/ui/element.hpp>

namespace sbx::ui {

class grid_layout : public layout {

public:

  std::uint32_t columns{2};
  std::uint32_t rows{2};
  std::float_t horizontal_spacing{0.0f};
  std::float_t vertical_spacing{0.0f};

  grid_layout() = default;

  grid_layout(std::uint32_t columns, std::uint32_t rows, std::float_t spacing = 0.0f)
  : columns{columns},
    rows{rows},
    horizontal_spacing{spacing},
    vertical_spacing{spacing} { }

  grid_layout(std::uint32_t columns, std::uint32_t rows, std::float_t horizontal_spacing, std::float_t vertical_spacing)
  : columns{columns},
    rows{rows},
    horizontal_spacing{horizontal_spacing},
    vertical_spacing{vertical_spacing} { }

  ~grid_layout() override = default;

  auto arrange(const rectangle& bounds, std::vector<std::unique_ptr<element>>& children) -> void override {
    const auto content = content_rect(bounds);

    if (children.empty() || columns == 0u || rows == 0u) {
      return;
    }

    const auto cell_width = (content.width - horizontal_spacing * static_cast<std::float_t>(columns - 1u)) / static_cast<std::float_t>(columns);
    const auto cell_height = (content.height - vertical_spacing * static_cast<std::float_t>(rows - 1u)) / static_cast<std::float_t>(rows);

    for (std::uint32_t i = 0u; i < children.size(); ++i) {
      const auto col = i % columns;
      const auto row = i / columns;

      if (row >= rows) {
        break;
      }

      const auto x = content.x + static_cast<std::float_t>(col) * (cell_width + horizontal_spacing);
      const auto y = content.y + content.height - static_cast<std::float_t>(row + 1u) * cell_height - static_cast<std::float_t>(row) * vertical_spacing;

      children[i]->resolve_as_arranged(rectangle{x, y, cell_width, cell_height});
    }
  }

}; // class grid_layout

} // namespace sbx::ui

#endif // LIBSBX_UI_GRID_LAYOUT_HPP_
