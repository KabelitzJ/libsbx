// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_SCENE_HIERARCHY_PANEL_HPP_
#define EDITOR_PANELS_SCENE_HIERARCHY_PANEL_HPP_

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_graph.hpp>

namespace editor {

class scene_hierarchy_panel {

public:

  scene_hierarchy_panel() = default;

  auto draw() -> void;

  auto selected_node() const -> sbx::scenes::node {
    return _selected_node;
  }

  auto set_selected_node(sbx::scenes::node node) -> void {
    _selected_node = node;
  }

private:

  auto _draw_node(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_create_node_popup(sbx::scenes::scene_graph& graph) -> void;

  sbx::scenes::node _selected_node{sbx::scenes::node::null};
  sbx::scenes::node _create_parent{sbx::scenes::node::null};
  bool _open_create_popup{false};
  std::array<char, 128> _name_buffer{};

}; // class scene_hierarchy_panel

} // namespace editor

#endif // EDITOR_PANELS_SCENE_HIERARCHY_PANEL_HPP_
