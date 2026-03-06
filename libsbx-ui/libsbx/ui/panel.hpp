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

  auto submit(const math::vector2& screen_size) -> void override;

}; // class panel

} // namespace sbx::ui

#endif // LIBSBX_UI_PANEL_HPP_
