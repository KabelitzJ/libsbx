// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_HIERARCHY_PANEL_HPP_
#define EDITOR_PANELS_HIERARCHY_PANEL_HPP_

#include <libsbx/ecs/entity.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/scene.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief The "Hierarchy" panel: a tree of the active scene's nodes. Clicking a row selects that
 * node in the shared editor_state; clicking empty space clears the selection. A toolbar button and
 * per-row context menu create/delete nodes.
 */
class hierarchy_panel final : public editor_panel {

public:

  auto draw(editor_state& state) -> void override;

private:

  auto _draw_node_row(editor_state& state, sbx::scenes::scene& scene, sbx::ecs::entity entity) -> void;

  // Set by a row's "Delete Node" context-menu item, resolved and destroyed once after the whole
  // tree has been walked (deleting mid-walk would mutate relationship.children out from under the
  // recursion). A uuid, not an entity — see editor_state::node_selection for why.
  sbx::math::uuid _pending_delete_id{sbx::math::uuid::nil()};

}; // class hierarchy_panel

} // namespace editor

#endif // EDITOR_PANELS_HIERARCHY_PANEL_HPP_
