// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_INSPECTOR_PANEL_HPP_
#define EDITOR_PANELS_INSPECTOR_PANEL_HPP_

#include <string>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_graph.hpp>

#include <libsbx/models/material.hpp>

#include <editor/panels/texture_cache.hpp>

namespace editor {

class inspector_panel {

public:

  explicit inspector_panel(texture_cache& cache)
  : _texture_cache{cache} { }

  auto draw(const sbx::scenes::node selected_node) -> void;

private:

  auto _draw_tag(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_transform(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_directional_light(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_point_light(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_camera(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_static_mesh(sbx::scenes::scene_graph& graph, sbx::scenes::node node) -> void;

  auto _draw_material(const sbx::math::uuid& material_id, std::uint32_t submesh_index) -> void;

  auto _draw_texture_slot(const char* label, sbx::models::texture_slot& slot) -> void;

  auto _draw_vector3_control(const std::string& label, sbx::math::vector3& values, float reset_value = 0.0f) -> bool;

  texture_cache& _texture_cache;

  std::string _tag_buffer;

}; // class inspector_panel

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_PANEL_HPP_
