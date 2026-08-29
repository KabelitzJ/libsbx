// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_HIERARCHY_PANEL_HPP_
#define EDITOR_PANELS_HIERARCHY_PANEL_HPP_

#include <libsbx/ecs/entity.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/scene.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief The "Hierarchy" panel: a tree of the active scene's nodes. Clicking a row selects that
 * node in the shared editor_state; clicking empty space clears the selection. A toolbar button and
 * per-row context menu create/delete nodes.
 */
class hierarchy_panel final : public editor_panel {

public:

  /** @brief The exact string passed to ImGui::Begin() — the window's identity (icon + label + ###id, all significant). Single source of truth: also referenced by editor_ui_layer's default dock layout, so a rename here can't silently desync it. */
  inline static constexpr auto window_name = ICON_MDI_FILE_TREE " Hierarchy###hierarchy_panel";

  auto draw(editor_state& state) -> void override;

private:

  auto _draw_node_row(editor_state& state, sbx::scenes::scene& scene, sbx::ecs::entity entity) -> void;

  sbx::math::uuid _pending_delete_id{sbx::math::uuid::nil()};
  sbx::math::uuid _pending_add_child_parent_id{sbx::math::uuid::nil()};

}; // class hierarchy_panel

} // namespace editor

#endif // EDITOR_PANELS_HIERARCHY_PANEL_HPP_
