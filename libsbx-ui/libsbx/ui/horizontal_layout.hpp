// SPDX-License-Identifier: MIT
#ifndef LIBSBX_UI_HORIZONTAL_LAYOUT_HPP_
#define LIBSBX_UI_HORIZONTAL_LAYOUT_HPP_

#include <algorithm>

#include <libsbx/ui/layout.hpp>
#include <libsbx/ui/element.hpp>

namespace sbx::ui {

class horizontal_layout : public layout {

public:

  std::float_t spacing{0.0f};

  horizontal_layout() = default;

  horizontal_layout(std::float_t spacing);

  ~horizontal_layout() override = default;

  auto arrange(const rectangle& bounds, std::vector<std::unique_ptr<element>>& children) -> void override;

}; // class horizontal_layout

} // namespace sbx::ui

#endif // LIBSBX_UI_HORIZONTAL_LAYOUT_HPP_
