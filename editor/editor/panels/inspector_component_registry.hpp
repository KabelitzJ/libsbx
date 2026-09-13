// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_INSPECTOR_COMPONENT_REGISTRY_HPP_
#define EDITOR_PANELS_INSPECTOR_COMPONENT_REGISTRY_HPP_

#include <vector>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <libsbx/assets/assets_module.hpp>

#include <libsbx/scripting/scripting_module.hpp>

#include <editor/editor_state.hpp>

namespace editor {

/**
 * @brief One entry per ECS component type the Inspector's "Add Component" menu offers, the single
 * source of truth inspector_panel::_draw_node_properties' dispatch loop and draw_add_component_menu
 * both drive off -- before this existed, the same ~29-type list was hand-spelled in both places
 * (plus the add menu's own mutual-exclusion checks), so adding a component type meant editing two
 * places by hand and easy to let drift.
 */
struct component_entry {
  const char* name;     // e.g. "Camera" -- add-menu label suffix, search-filter text, "Add "+name command label
  const char* icon;     // e.g. ICON_MDI_CAMERA_OUTLINE -- add-menu item icon
  const char* category; // "Common" | "3D" | "2D" -- which add-menu submenu this falls under

  // Entries sharing a nonzero exclusion_group are mutually exclusive in the Add menu (e.g. Shape
  // Collider / Mesh Collider) -- narrowphase (or layout resolution) only ever wants one per node.
  int exclusion_group{0};

  bool (*has)(const sbx::scenes::node&);
  void (*add)(editor_state&, sbx::scenes::scene&, const sbx::scenes::node&, const std::string&);
  void (*draw)(editor_state&, sbx::scenes::scene&, sbx::scenes::node&, sbx::assets::assets_module&);
}; // struct component_entry

/** @brief Every entry, in the exact order _draw_node_properties draws them. */
auto component_entries() -> const std::vector<component_entry>&;

/** @brief The "Add Component" button + popup menu, grouped into Common/3D/2D by component_entries()'s category, plus an open-ended Script submenu (scripts aren't native components, so they aren't in the table -- discovered live from the compiled game assembly instead). */
auto draw_add_component_menu(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::scripting::scripting_module& scripting_module) -> void;

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_COMPONENT_REGISTRY_HPP_
