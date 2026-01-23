// SPDX-License-Identifier: MIT
#ifndef LIBSBX_EDITOR_PROPERTY_PANEL_HPP_
#define LIBSBX_EDITOR_PROPERTY_PANEL_HPP_

#include <array>

#include <libsbx/math/uuid.hpp>

namespace sbx::editor {

class property_panel {

public:

  property_panel();

  auto render(const math::uuid& selected_node_id) -> void;

private:

  std::array<char, 32u> _name_buffer{};

}; // class property_panel


} // namespace sbx::editor

#endif // LIBSBX_EDITOR_PROPERTY_PANEL_HPP_