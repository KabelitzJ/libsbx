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

/** @brief One component type the Inspector offers; the single table driving both the section drawing and the Add Component menu. */
struct component_entry {
  const char* name;     // e.g. "Camera": menu label, search text and command label
  const char* icon;     // e.g. ICON_MDI_CAMERA_OUTLINE
  const char* category; // "Common", "3D" or "2D" submenu

  // Entries sharing a nonzero group are mutually exclusive in the Add menu, e.g. Shape and Mesh Collider.
  int exclusion_group{0};

  bool (*has)(const sbx::scenes::node&);
  void (*add)(editor_state&, sbx::scenes::scene&, const sbx::scenes::node&, const std::string&);
  void (*draw)(editor_state&, sbx::scenes::scene&, sbx::scenes::node&, sbx::assets::assets_module&);
}; // struct component_entry

/**
 * @brief Every entry, in the order the Inspector draws them.
 *
 * @return The entries.
 */
auto component_entries() -> const std::vector<component_entry>&;

/** @brief The Add Component button and popup, grouped by category, plus a Script submenu discovered from the compiled game assembly. */
auto draw_add_component_menu(editor_state& state, sbx::scenes::scene& target, sbx::scenes::node& node, sbx::scripting::scripting_module& scripting_module) -> void;

} // namespace editor

#endif // EDITOR_PANELS_INSPECTOR_COMPONENT_REGISTRY_HPP_
