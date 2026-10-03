// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_HIERARCHY_PANEL_HPP_
#define EDITOR_PANELS_HIERARCHY_PANEL_HPP_

#include <array>
#include <optional>
#include <unordered_set>
#include <vector>

#include <libsbx/ecs/entity.hpp>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/scene.hpp>

#include <libsbx/render/ui/fonts/material_design_icons.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/** @brief The Hierarchy: a tree of the active scene's nodes with selection, drag-and-drop reordering and create/delete menus. */
class hierarchy_panel final : public editor_panel {

public:

  // The panel's ImGui::Begin() string, also used by the default dock layout.
  inline static constexpr auto window_name = ICON_MDI_FILE_TREE " Hierarchy###hierarchy_panel";

  auto draw(editor_state& state) -> void override;

private:

  /**
   * @brief Draws one row. Dragging onto its top or bottom third inserts before or after it; the middle third reparents under it.
   *
   * @param state The editor state.
   * @param scene The scene.
   * @param entity The row's node.
   * @param parent_id The node's parent, or nullopt at the top level.
   * @param sibling_index The node's index among its siblings.
   */
  auto _draw_node_row(editor_state& state, sbx::scenes::scene& scene, sbx::ecs::entity entity, std::optional<sbx::math::uuid> parent_id, std::size_t sibling_index) -> void;

  auto _draw_child_rows(editor_state& state, sbx::scenes::scene& scene, sbx::math::uuid parent_id, const std::vector<sbx::ecs::entity>& children) -> void;

  /**
   * @brief Validates a drop of the dragged set (the payload, or the whole selection if it contains it) and queues it in _pending_reparent.
   *
   * Never mutates the scene: drop targets run mid-traversal, and changing children vectors there breaks the walk and ImGui's id stack.
   *
   * @param state The editor state.
   * @param scene The scene.
   * @param payload_id The dragged node.
   * @param new_parent_id The new parent, or nullopt for the top level.
   * @param new_index The index among the new siblings.
   */
  auto _try_reparent(editor_state& state, sbx::scenes::scene& scene, sbx::math::uuid payload_id, std::optional<sbx::math::uuid> new_parent_id, std::size_t new_index) -> void;

  /**
   * @brief The node's current parent.
   *
   * @param scene The scene.
   * @param id The node.
   *
   * @return The parent's id, or nullopt if invalid, top-level or unresolvable.
   */
  [[nodiscard]] auto _current_parent_id(sbx::scenes::scene& scene, sbx::math::uuid id) const -> std::optional<sbx::math::uuid>;

  /** @brief Rebuilds _search_visible_ids from the search text; empty means no filter. */
  auto _update_search_matches(sbx::scenes::scene& scene) -> void;

  /** @brief Applies the queued reparent as one command, or one composite for a multi-drag. */
  auto _apply_pending_reparent(editor_state& state, sbx::scenes::scene& scene) -> void;

  auto _begin_rename(const sbx::scenes::node& node) -> void;

  auto _commit_rename(editor_state& state, sbx::scenes::scene& scene, sbx::scenes::node& node) -> void;

  /** @brief "Apply to Prefab"/"Revert to Prefab" submenus per overridden component, plus Apply All/Revert All. Draws nothing without overrides. */
  auto _draw_prefab_override_menu(sbx::scenes::scene& scene, const sbx::scenes::node& node) -> void;

  /** @brief "Add Node" and the 3D Object submenu, shared by both empty-space context menus. */
  auto _draw_empty_space_context_menu(editor_state& state, sbx::scenes::scene& scene) -> void;

  sbx::math::uuid _pending_delete_id{sbx::math::uuid::nil()};
  void (*_pending_node_action)(editor_state&, sbx::scenes::scene&){nullptr}; // run after the tree walk, like _pending_delete_id
  sbx::math::uuid _pending_add_child_parent_id{sbx::math::uuid::nil()};

  /** @brief A reparent request, applied after the tree finishes drawing. */
  struct pending_reparent {
    std::vector<sbx::math::uuid> dragged_ids;
    std::optional<sbx::math::uuid> new_parent_id;
    std::size_t new_index;
  };

  std::optional<pending_reparent> _pending_reparent{};

  /** @brief A Shift+click range select, applied after the tree finishes drawing. */
  struct pending_range_select {
    sbx::math::uuid anchor_id;
    sbx::math::uuid clicked_id;
  };

  std::optional<pending_range_select> _pending_range_select{};

  // Rows drawn this frame, top to bottom, for range selects and ordering a multi-drag.
  std::vector<sbx::math::uuid> _visible_row_order{};

  sbx::math::uuid _renaming_id{sbx::math::uuid::nil()};
  std::array<char, 256u> _rename_buffer{};
  std::array<char, 128u> _search_buffer{};
  std::unordered_set<sbx::math::uuid> _search_visible_ids{}; // matches plus their ancestors
  bool _rename_focus_pending{false};

  // The row a plain click will select on release, unless the press becomes a drag; selecting on press would change the selection under the drag.
  sbx::math::uuid _deferred_click_id{sbx::math::uuid::nil()};
  bool _deferred_click_became_drag{false};

}; // class hierarchy_panel

} // namespace editor

#endif // EDITOR_PANELS_HIERARCHY_PANEL_HPP_
