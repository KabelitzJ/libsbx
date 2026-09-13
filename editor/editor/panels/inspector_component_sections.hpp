// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_INSPECTOR_COMPONENT_SECTIONS_HPP_
#define EDITOR_PANELS_INSPECTOR_COMPONENT_SECTIONS_HPP_

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief One draw_*_section per ECS component type the Inspector edits: draws a CollapsingHeader
 * (unchecking it removes the component), and while expanded, that component's fields. Driven by
 * inspector_panel::_draw_node_properties via component_entries() (see
 * inspector_component_registry.hpp) rather than called directly, except by that dispatch loop.
 */
auto draw_camera_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_mesh_renderer_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void;
auto draw_animator_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_directional_light_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_point_light_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_spot_light_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_skybox_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void;
auto draw_particle_effect_instance_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void;
auto draw_rigidbody_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_nav_agent_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_shape_collider_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_mesh_collider_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::assets::assets_module& assets_module) -> void;
auto draw_canvas_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_canvas_group_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_canvas_scaler_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_rect_transform_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_ui_image_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_ui_text_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_ui_button_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_layout_element_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_content_size_fitter_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_horizontal_layout_group_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_vertical_layout_group_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_grid_layout_group_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_ui_mask_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_ui_toggle_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_ui_slider_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_ui_scrollbar_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;
auto draw_ui_scroll_rect_section(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node) -> void;

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_COMPONENT_SECTIONS_HPP_
