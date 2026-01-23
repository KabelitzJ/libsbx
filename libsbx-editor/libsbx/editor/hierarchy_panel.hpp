// SPDX-License-Identifier: MIT
#ifndef LIBSBX_EDITOR_HIERARCHY_PANEL_HPP_
#define LIBSBX_EDITOR_HIERARCHY_PANEL_HPP_

#include <libsbx/utility/enum.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/node.hpp>

namespace sbx::editor {

class hierarchy_panel {

  enum class popup : std::uint8_t {
    new_node = utility::bit_v<0u>,
    add_component = utility::bit_v<1u>,
    delete_node = utility::bit_v<2u>
  }; // enum class popup

public:

  hierarchy_panel();

  auto render() -> void;

  auto selected_node_id() const -> const math::uuid;

private:

  auto _context_menu() -> void;

  auto _new_node_popup() -> void;

  auto _add_component_popup() -> void;

  auto _delete_node_popup() -> void;

  auto _build_tree(const sbx::scenes::node node) -> void;

  math::uuid _selected_node_id;
  utility::bit_field<popup> _open_popups;
  std::array<char, 32> _new_name_buffer;

}; // class hierarchy_panel

} // namespace sbx::editor

#endif // LIBSBX_EDITOR_HIERARCHY_PANEL_HPP_