// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_STATE_HPP_
#define EDITOR_EDITOR_STATE_HPP_

#include <cstddef>
#include <filesystem>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <memory>
#include <string>

#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <editor/commands/command.hpp>
#include <editor/commands/command_stack.hpp>

namespace editor {

/**
 * @brief Drag payload type for a Hierarchy row (a scene node). Shared here rather than kept local
 * to hierarchy_panel.cpp — reordering/reparenting inside the Hierarchy panel is the drag *source*,
 * but a "drop a node here to assign it" *target* is now also useful elsewhere (a Node-typed script
 * field slot in the Inspector).
 */
inline constexpr auto node_drag_drop_payload_type = "HIERARCHY_NODE";

/**
 * @brief What kind of asset a file in the project's assets directory is, inferred from extension.
 */
enum class asset_kind {
  unknown,
  texture,
  mesh,
  material,
  environment_map,
  particle_effect,
  animation_graph,
  shader_graph,
  font,
  prefab,
  scene, // .scene, no cook step but still manifest-registered — same treatment as prefab
  script, // .cs, reference-only: compiled by scripting::script_compiler, not assets_module::import
}; // enum class asset_kind

/** @brief Nothing is selected. */
struct empty_selection { };

/**
 * @brief One or more scene nodes are selected, identified by uuid.
 *
 * ids is in selection order — ids.back() is the "primary" node (most recently added), used
 * wherever exactly one node is still needed (Inspector single-selection view, F2 rename target,
 * the gizmo's Local-mode orientation). range_anchor is the pivot the Hierarchy panel's next
 * Shift+click range-select measures from; it isn't necessarily a member of ids any more (e.g.
 * after Ctrl+click removes it), it's just the last row that was explicitly clicked/toggled.
 */
struct node_selection {
  std::vector<sbx::math::uuid> ids{};
  sbx::math::uuid range_anchor{sbx::math::uuid::nil()};
}; // struct node_selection

/** @brief An asset file is selected, from the Asset Browser. */
struct asset_selection {
  sbx::math::uuid id{sbx::math::uuid::nil()};
  std::filesystem::path path{};               // project-relative to the active project's assets directory
  asset_kind kind{asset_kind::unknown};
}; // struct asset_selection

using selection = std::variant<empty_selection, node_selection, asset_selection>;

/** @brief Which handles the viewport gizmo shows for the current selection. */
enum class gizmo_operation {
  translate,
  rotate,
  scale,
}; // enum class gizmo_operation

/** @brief Whether the gizmo manipulates in the selected node's local axes or world axes. */
enum class gizmo_mode {
  local,
  world,
}; // enum class gizmo_mode

/**
 * @brief State shared across editor panels: what's currently selected (a node or an asset), plus
 * the viewport gizmo's operation/mode. Anything private to a single panel lives on that panel
 * instead (see editor_panel) — this only holds what genuinely crosses panel boundaries.
 */
struct editor_state {

  selection current_selection{empty_selection{}};
  gizmo_operation current_gizmo_operation{gizmo_operation::translate};
  gizmo_mode current_gizmo_mode{gizmo_mode::world};

  /**
   * @brief Re-resolves the primary selected node (node_selection::ids.back(), i.e. the most
   * recently added), if any, against @p scene, via its uuid. Returns an invalid node if nothing is
   * selected, the selection isn't a node, or no node with that uuid exists any more (e.g. it was
   * deleted). Identical to a single-node selection's only node when exactly one is selected.
   */
  [[nodiscard]] auto selected_node(sbx::scenes::scene& scene) const -> sbx::scenes::node;

  /** @brief Whether @p node is anywhere in the current node selection (membership test, not "is the primary"). */
  [[nodiscard]] auto is_node_selected(const sbx::scenes::node& node) const noexcept -> bool;

  /** @brief Replaces the selection with just @p node, and makes it the new range-select anchor. */
  auto select_node(const sbx::scenes::node& node) -> void;

  /** @brief Ctrl+click: adds @p node to the selection if absent, removes it if present. Always updates the range-select anchor. */
  auto toggle_node_selection(const sbx::scenes::node& node) -> void;

  /** @brief Shift+click in the viewport (no list order to range over there): adds @p node to the selection if absent, never removes. Always updates the range-select anchor. */
  auto add_node_to_selection(const sbx::scenes::node& node) -> void;

  /** @brief Replaces the selection with exactly @p ids, in that order, without touching the range-select anchor. Empty @p ids clears the selection. Used by the Hierarchy panel's Shift range-select. */
  auto set_node_selection(std::vector<sbx::math::uuid> ids) -> void;

  /** @brief The current node selection's ids, in selection order; empty if nothing/an asset is selected. */
  [[nodiscard]] auto selected_node_ids() const -> const std::vector<sbx::math::uuid>&;

  /** @brief The Hierarchy panel's next Shift+click range-select pivot; nil if no node selection is active. */
  [[nodiscard]] auto node_selection_anchor() const -> sbx::math::uuid;

  /** @brief Number of currently selected nodes (0 if nothing/an asset is selected). */
  [[nodiscard]] auto selected_node_count() const -> std::size_t;

  auto select_asset(sbx::math::uuid id, std::filesystem::path path, asset_kind kind) -> void;

  auto clear_selection() -> void;

  /**
   * @brief One-shot "show in Asset Browser" request: asks the Asset Browser to navigate to and
   * expand the folder containing @p path (project-relative), without changing what's currently
   * selected/shown in the Inspector. Consumed (reset to nullopt) once asset_browser_panel acts on
   * it, so it fires exactly once per request rather than pinning the browser to that folder.
   */
  auto request_reveal_in_browser(std::filesystem::path path) -> void {
    reveal_in_browser_request = std::move(path);
  }

  std::optional<std::filesystem::path> reveal_in_browser_request{};

  /** @brief One-shot "open the visual graph editor" request, same shape as reveal_in_browser_request — consumed by animation_graph_panel once it acts on it. */
  struct animation_graph_edit_request {
    sbx::math::uuid id{sbx::math::uuid::nil()};
    std::filesystem::path path{}; // project-relative
    sbx::math::uuid preview_mesh_id{sbx::math::uuid::nil()}; // nil = unknown -- caller had no mesh in context (opened from the Asset Browser rather than a node's Animator component)
  }; // struct animation_graph_edit_request

  auto request_open_animation_graph_editor(sbx::math::uuid id, std::filesystem::path path, sbx::math::uuid preview_mesh_id = sbx::math::uuid::nil()) -> void {
    open_animation_graph_request = animation_graph_edit_request{id, std::move(path), preview_mesh_id};
  }

  std::optional<animation_graph_edit_request> open_animation_graph_request{};

  /** @brief Same idea as animation_graph_edit_request, consumed by shader_graph_panel. */
  struct shader_graph_edit_request {
    sbx::math::uuid id{sbx::math::uuid::nil()};
    std::filesystem::path path{}; // project-relative
  }; // struct shader_graph_edit_request

  auto request_open_shader_graph_editor(sbx::math::uuid id, std::filesystem::path path) -> void {
    open_shader_graph_request = shader_graph_edit_request{id, std::move(path)};
  }

  std::optional<shader_graph_edit_request> open_shader_graph_request{};

  /** @brief Same idea as animation_graph_edit_request, consumed by editor_ui_layer (routes into its own guarded open_scene(), see editor_ui_layer::open_scene's doc comment). */
  struct scene_edit_request {
    std::filesystem::path path{}; // project-relative
  }; // struct scene_edit_request

  auto request_open_scene(std::filesystem::path path) -> void {
    open_scene_request = scene_edit_request{std::move(path)};
  }

  std::optional<scene_edit_request> open_scene_request{};

  /** @brief One-shot "open the Edit Layers... popup" request -- fired from the node Layer dropdown or a LayerMask field's popup, consumed by editor_ui_layer once it draws the popup. */
  auto request_open_edit_layers_popup() -> void {
    open_edit_layers_popup_request = true;
  }

  bool open_edit_layers_popup_request{false};

  // The scene-graph undo/redo history, shared across panels like current_selection. Prefer the
  // pass-throughs below over reaching into this directly.
  command_stack commands{};

  auto push_command(sbx::scenes::scene& target, std::unique_ptr<command> cmd) -> void {
    commands.push(target, std::move(cmd));
  }

  auto undo(sbx::scenes::scene& target) -> void {
    commands.undo(target);
  }

  auto redo(sbx::scenes::scene& target) -> void {
    commands.redo(target);
  }

  [[nodiscard]] auto can_undo() const noexcept -> bool {
    return commands.can_undo();
  }

  [[nodiscard]] auto can_redo() const noexcept -> bool {
    return commands.can_redo();
  }

  [[nodiscard]] auto undo_label() const -> std::string {
    return commands.undo_label();
  }

  [[nodiscard]] auto redo_label() const -> std::string {
    return commands.redo_label();
  }

  auto clear_command_stack() -> void {
    commands.clear();
  }

}; // struct editor_state

} // namespace editor

#endif // EDITOR_EDITOR_STATE_HPP_
