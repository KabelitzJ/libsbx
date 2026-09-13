// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_HIERARCHY_PANEL_HPP_
#define EDITOR_PANELS_HIERARCHY_PANEL_HPP_

#include <array>
#include <optional>
#include <vector>

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

  /**
   * @brief Draws one row. parent_id/sibling_index are this node's own position among its siblings
   * (nullopt parent_id = top-level) — not read for anything but the row's own drag-drop target,
   * which uses them to reorder without any extra widget: hovering the top/bottom third of the row
   * during a drag inserts before/after it (same parent_id, sibling_index / sibling_index + 1); the
   * middle third reparents the drag as this row's child instead. A stock TreeNodeEx row's own rect
   * is the whole hit area, so this adds no extra height anywhere in the tree.
   */
  auto _draw_node_row(editor_state& state, sbx::scenes::scene& scene, sbx::ecs::entity entity, std::optional<sbx::math::uuid> parent_id, std::size_t sibling_index) -> void;

  auto _draw_child_rows(editor_state& state, sbx::scenes::scene& scene, sbx::math::uuid parent_id, const std::vector<sbx::ecs::entity>& children) -> void;

  /**
   * @brief Resolves the dragged set (the payload's node, or the whole current selection if the
   * payload's node is part of it), validates the drop (no self-parenting, no dropping onto a
   * descendant of any dragged node) and, if sound, records it into _pending_reparent for draw() to
   * apply once the tree is done drawing.
   *
   * Never mutates the scene graph itself: a drop target is evaluated mid-traversal, and applying
   * scene::insert_child() right there would resize/erase a relationship::children vector that an
   * enclosing _draw_child_rows call still holds a reference into and is mid-iteration over —
   * exactly the kind of container mutation ImGui's tree/ID stack can't survive (surfaces as
   * "Missing TreePop()" / "PopID() called too many times").
   */
  auto _try_reparent(editor_state& state, sbx::scenes::scene& scene, sbx::math::uuid payload_id, std::optional<sbx::math::uuid> new_parent_id, std::size_t new_index) -> void;

  /** @brief id's current parent, or nullopt if id is invalid, top-level, or unresolvable (same "resolve via relationship::parent" pattern reparent_node_command's constructor uses). */
  [[nodiscard]] auto _current_parent_id(sbx::scenes::scene& scene, sbx::math::uuid id) const -> std::optional<sbx::math::uuid>;

  /** @brief Keeps only the ids in @p ids whose parent isn't itself also in @p ids — moving/deleting an ancestor already carries its selected descendants along, so they'd otherwise be handled twice. */
  [[nodiscard]] auto _filter_to_selection_roots(sbx::scenes::scene& scene, const std::vector<sbx::math::uuid>& ids) const -> std::vector<sbx::math::uuid>;

  /** @brief Applies _pending_reparent (if any), as one reparent_node_command or, for a multi-drag, one composite_command batching a reparent_node_command per dragged root -- see _try_reparent's doc comment for why this can't happen mid-traversal. No-op if nothing is pending. */
  auto _apply_pending_reparent(editor_state& state, sbx::scenes::scene& scene) -> void;

  auto _begin_rename(const sbx::scenes::node& node) -> void;

  auto _commit_rename(editor_state& state, sbx::scenes::scene& scene, sbx::scenes::node& node) -> void;

  /** @brief "Apply to Prefab"/"Revert to Prefab" submenus, one entry per component node's own overrides (see scene_serializer::prefab_overrides_of), plus Apply All/Revert All. No-op (draws nothing) if node isn't part of a prefab instance or has no overrides. */
  auto _draw_prefab_override_menu(sbx::scenes::scene& scene, const sbx::scenes::node& node) -> void;

  /** @brief "Add Node" + the 3D Object submenu, shared by the empty-space InvisibleButton's context menu and the window background's context menu -- the two places a click lands on nothing in particular. */
  auto _draw_empty_space_context_menu(editor_state& state, sbx::scenes::scene& scene) -> void;

  sbx::math::uuid _pending_delete_id{sbx::math::uuid::nil()};
  std::vector<sbx::math::uuid> _pending_delete_ids{}; // multi-select delete; _pending_delete_id above still handles the single-node case unchanged
  sbx::math::uuid _pending_add_child_parent_id{sbx::math::uuid::nil()};

  /** @brief One drag/drop reparent request, applied after the tree has fully drawn this frame (see _try_reparent). */
  struct pending_reparent {
    std::vector<sbx::math::uuid> dragged_ids;
    std::optional<sbx::math::uuid> new_parent_id;
    std::size_t new_index;
  };

  std::optional<pending_reparent> _pending_reparent{};

  /** @brief One Shift+click range-select request, applied after the tree has fully drawn this frame (needs _visible_row_order complete). */
  struct pending_range_select {
    sbx::math::uuid anchor_id;
    sbx::math::uuid clicked_id;
  };

  std::optional<pending_range_select> _pending_range_select{};

  /** @brief Every row actually drawn this frame, in visual top-to-bottom order. Rebuilt (cleared, then repopulated) each draw() call; used to resolve Shift range-selects and to order a multi-drag batch. */
  std::vector<sbx::math::uuid> _visible_row_order{};

  sbx::math::uuid _renaming_id{sbx::math::uuid::nil()};
  std::array<char, 256u> _rename_buffer{};
  bool _rename_focus_pending{false};

  /**
   * @brief Press-time candidate for this row's plain-click selection (a fresh single select, a
   * multi-selection collapsing down to just this row, whatever it resolves to), applied on release
   * only if the press never turned into a drag (see _draw_node_row). Never selected immediately on
   * press — that would flip the selection (and whatever's driven by it, e.g. the Inspector) over to
   * this row before BeginDragDropSource ever got a chance to see the drag, the exact
   * asset_tile.cpp IsItemClicked-vs-drag pitfall, just without an InvisibleButton return value to
   * lean on here.
   */
  sbx::math::uuid _deferred_click_id{sbx::math::uuid::nil()};
  bool _deferred_click_became_drag{false};

}; // class hierarchy_panel

} // namespace editor

#endif // EDITOR_PANELS_HIERARCHY_PANEL_HPP_
