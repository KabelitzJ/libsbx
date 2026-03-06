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

  grid_layout(std::uint32_t columns, std::uint32_t rows, std::float_t spacing = 0.0f);

  grid_layout(std::uint32_t columns, std::uint32_t rows, std::float_t horizontal_spacing, std::float_t vertical_spacing);

  ~grid_layout() override = default;

  auto arrange(const rectangle& bounds, std::vector<std::unique_ptr<element>>& children) -> void override;

}; // class grid_layout

} // namespace sbx::ui

#endif // LIBSBX_UI_GRID_LAYOUT_HPP_
