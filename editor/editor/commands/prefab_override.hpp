// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_COMMANDS_PREFAB_OVERRIDE_HPP_
#define EDITOR_COMMANDS_PREFAB_OVERRIDE_HPP_

#include <concepts>
#include <string_view>

#include <libsbx/scenes/components.hpp>
#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene_serializer.hpp>

#include <libsbx/canvas/components.hpp>

#include <libsbx/physics/rigidbody.hpp>
#include <libsbx/physics/collider.hpp>
#include <libsbx/physics/nav/nav_agent.hpp>

namespace editor {

// Component -> scene_serializer's own "type" string for it (see scene_serializer.cpp's write_node/
// read_node_components) -- the single source of truth those already use, just made reachable by
// C++ type from here too. A component with no entry (the "" fallback) simply isn't tracked by the
// prefab override system: editing it is never recorded as an override, so a prefab_instance's copy
// always just follows the prefab for that component.
template<typename Component>
constexpr auto component_key() -> std::string_view {
  if constexpr (std::same_as<Component, sbx::scenes::local_transform>) { return "transform"; }
  else if constexpr (std::same_as<Component, sbx::scenes::layer>) { return "layer"; }
  else if constexpr (std::same_as<Component, sbx::scenes::mesh_renderer>) { return "static_mesh"; }
  else if constexpr (std::same_as<Component, sbx::scenes::animator>) { return "animator"; }
  else if constexpr (std::same_as<Component, sbx::scenes::camera>) { return "camera"; }
  else if constexpr (std::same_as<Component, sbx::scenes::directional_light>) { return "directional_light"; }
  else if constexpr (std::same_as<Component, sbx::scenes::point_light>) { return "point_light"; }
  else if constexpr (std::same_as<Component, sbx::scenes::spot_light>) { return "spot_light"; }
  else if constexpr (std::same_as<Component, sbx::scenes::skybox>) { return "skybox"; }
  else if constexpr (std::same_as<Component, sbx::scenes::particle_effect>) { return "particle_effect"; }
  else if constexpr (std::same_as<Component, sbx::canvas::canvas>) { return "canvas"; }
  else if constexpr (std::same_as<Component, sbx::canvas::canvas_scaler>) { return "canvas_scaler"; }
  else if constexpr (std::same_as<Component, sbx::canvas::rect_transform>) { return "rect_transform"; }
  else if constexpr (std::same_as<Component, sbx::canvas::canvas_group>) { return "canvas_group"; }
  else if constexpr (std::same_as<Component, sbx::canvas::ui_image>) { return "ui_image"; }
  else if constexpr (std::same_as<Component, sbx::canvas::ui_text>) { return "ui_text"; }
  else if constexpr (std::same_as<Component, sbx::canvas::ui_button>) { return "ui_button"; }
  else if constexpr (std::same_as<Component, sbx::canvas::ui_toggle>) { return "ui_toggle"; }
  else if constexpr (std::same_as<Component, sbx::canvas::ui_slider>) { return "ui_slider"; }
  else if constexpr (std::same_as<Component, sbx::canvas::ui_scrollbar>) { return "ui_scrollbar"; }
  else if constexpr (std::same_as<Component, sbx::canvas::ui_scroll_rect>) { return "ui_scroll_rect"; }
  else if constexpr (std::same_as<Component, sbx::canvas::layout_element>) { return "layout_element"; }
  else if constexpr (std::same_as<Component, sbx::canvas::content_size_fitter>) { return "content_size_fitter"; }
  else if constexpr (std::same_as<Component, sbx::canvas::horizontal_layout_group>) { return "horizontal_layout_group"; }
  else if constexpr (std::same_as<Component, sbx::canvas::vertical_layout_group>) { return "vertical_layout_group"; }
  else if constexpr (std::same_as<Component, sbx::canvas::grid_layout_group>) { return "grid_layout_group"; }
  else if constexpr (std::same_as<Component, sbx::canvas::ui_mask>) { return "ui_mask"; }
  else if constexpr (std::same_as<Component, sbx::scenes::script_component>) { return "script"; }
  else if constexpr (std::same_as<Component, sbx::physics::rigidbody>) { return "rigidbody"; }
  else if constexpr (std::same_as<Component, sbx::physics::nav_agent>) { return "nav_agent"; }
  else if constexpr (std::same_as<Component, sbx::physics::shape_collider>) { return "shape_collider"; }
  else if constexpr (std::same_as<Component, sbx::physics::mesh_collider>) { return "mesh_collider"; }
  else { return std::string_view{}; }
}

/**
 * @brief The override-recording hook every property-edit command (component_commands.hpp) calls
 * into. No-op if Component isn't a tracked type (component_key<Component>() is empty) or
 * member_node isn't part of a prefab instance. scene is whatever the calling command's own
 * execute()/undo() was given — never resolved internally (see command.hpp's doc comment).
 */
template<typename Component>
auto mark_prefab_override(sbx::scenes::scene& scene, const sbx::scenes::node& member_node, sbx::scenes::prefab_override_kind kind) -> void {
  if (const auto key = component_key<Component>(); !key.empty()) {
    sbx::scenes::scene_serializer::mark_prefab_override(scene, member_node, key, kind);
  }
}

} // namespace editor

#endif // EDITOR_COMMANDS_PREFAB_OVERRIDE_HPP_
