// SPDX-License-Identifier: MIT
#ifndef EDITOR_PANELS_INSPECTOR_PANEL_HPP_
#define EDITOR_PANELS_INSPECTOR_PANEL_HPP_

#include <string>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/scene.hpp>
#include <libsbx/scenes/scene_graph.hpp>

#include <libsbx/scenes/components/directional_light.hpp>
#include <libsbx/scenes/components/point_light.hpp>
#include <libsbx/scenes/components/camera.hpp>
#include <libsbx/scenes/components/static_mesh.hpp>

#include <libsbx/models/material.hpp>

#include <editor/panels/component_registry.hpp>
#include <editor/panels/texture_cache.hpp>

namespace editor {

class inspector_panel {

public:

  explicit inspector_panel(texture_cache& cache);

  auto draw(const sbx::scenes::node selected_node) -> void;

  auto components() -> component_registry& {
    return _components;
  }

  auto components() const -> const component_registry& {
    return _components;
  }

private:

  auto _register_default_components() -> void;

  auto _draw_tag(sbx::scenes::node node) -> void;

  auto _draw_transform(sbx::scenes::node node) -> void;

  auto _draw_components(sbx::scenes::node node) -> void;

  auto _draw_add_component_button(sbx::scenes::node node) -> void;

  auto _draw_directional_light(sbx::scenes::node node, sbx::scenes::directional_light& light) -> void;

  auto _draw_point_light(sbx::scenes::node node, sbx::scenes::point_light& light) -> void;

  auto _draw_camera(sbx::scenes::node node, sbx::scenes::camera& cam) -> void;

  auto _draw_static_mesh(sbx::scenes::node node, sbx::scenes::static_mesh& mesh) -> void;

  auto _draw_material(const sbx::math::uuid& material_id, std::uint32_t submesh_index) -> void;

  auto _draw_texture_slot(const char* label, sbx::models::texture_slot& slot) -> void;

  auto _draw_vector3_control(const std::string& label, sbx::math::vector3& values, float reset_value = 0.0f) -> bool;

  texture_cache& _texture_cache;

  std::string _tag_buffer;

  component_registry _components;

}; // class inspector_panel

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_PANEL_HPP_
