// SPDX-License-Identifier: MIT
#include <libsbx/ui/grid_layout.hpp>

namespace sbx::ui {

grid_layout::grid_layout(std::uint32_t columns, std::uint32_t rows, std::float_t spacing)
: columns{columns},
  rows{rows},
  horizontal_spacing{spacing},
  vertical_spacing{spacing} { }

grid_layout::grid_layout(std::uint32_t columns, std::uint32_t rows, std::float_t horizontal_spacing, std::float_t vertical_spacing)
: columns{columns},
  rows{rows},
  horizontal_spacing{horizontal_spacing},
  vertical_spacing{vertical_spacing} { }

auto grid_layout::arrange(const rectangle& bounds, std::vector<std::unique_ptr<element>>& children) -> void {
  const auto content = content_rectangle(bounds);

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

} // namespace sbx::ui