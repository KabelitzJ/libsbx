// SPDX-License-Identifier: MIT
#ifndef EDITOR_SELECTION_HPP_
#define EDITOR_SELECTION_HPP_

#include <libsbx/scenes/node.hpp>

namespace editor {

class selection {

public:

  selection() = delete;

  static auto selected_node() -> sbx::scenes::node {
    return _selected_node;
  }

  static auto set_selected_node(sbx::scenes::node node) -> void {
    _selected_node = node;
  }

private:

  inline static sbx::scenes::node _selected_node{sbx::scenes::node::null};

}; // class selection

} // namespace editor

#endif // EDITOR_SELECTION_HPP_
