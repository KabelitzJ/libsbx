// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_INSPECTOR_PANEL_HPP_
#define EDITOR_PANELS_INSPECTOR_PANEL_HPP_

#include <string>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_graph.hpp>

namespace editor {

class inspector_panel {

public:

  inspector_panel() = default;

  auto draw(const sbx::scenes::node selected_node) -> void;

private:

  auto _draw_transform(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_directional_light(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_point_light(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_camera(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_static_mesh(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_vec3_control(const std::string& label, sbx::math::vector3& values, float reset_value = 0.0f) -> bool;

  std::string _tag_buffer;

}; // class inspector_panel

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_PANEL_HPP_
