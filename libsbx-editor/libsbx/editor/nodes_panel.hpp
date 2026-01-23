// SPDX-License-Identifier: MIT
#ifndef LIBSBX_EDITOR_NODES_PANEL_HPP_
#define LIBSBX_EDITOR_NODES_PANEL_HPP_

#include <cinttypes>

#include <vector>

namespace sbx::editor {

class nodes_panel {

public:

  nodes_panel();

  auto render() -> void;

private:

  struct link {
    std::int32_t start;    
    std::int32_t end;    
  }; // struct link

  std::vector<link> _links;

}; // class nodes_panel

} // namespace sbx::editor

#endif // LIBSBX_EDITOR_NODES_PANEL_HPP_