// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_hierarchy_panel_HPP_
#define EDITOR_PANELS_hierarchy_panel_HPP_

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_graph.hpp>

#include <editor/editor_module.hpp>

namespace editor {

class hierarchy_panel {

public:

  hierarchy_panel() = default;

  auto draw() -> void;

private:

  auto _draw_node(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_create_node_popup(sbx::scenes::scene_graph& graph) -> void;

  sbx::scenes::node _create_parent{sbx::scenes::node::null};
  bool _open_create_popup{false};
  std::array<char, 128> _name_buffer{};

}; // class hierarchy_panel

} // namespace editor

#endif // EDITOR_PANELS_hierarchy_panel_HPP_
