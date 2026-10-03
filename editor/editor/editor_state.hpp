// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_EDITOR_STATE_HPP_
#define EDITOR_EDITOR_STATE_HPP_

#include <cstddef>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <memory>
#include <string>

#include <yaml-cpp/yaml.h>

#include <libsbx/math/matrix4x4.hpp>
#include <libsbx/math/uuid.hpp>

#include <libsbx/scenes/components.hpp>

#include <libsbx/scenes/node.hpp>
#include <libsbx/scenes/scene.hpp>

#include <editor/commands/command.hpp>
#include <editor/commands/command_stack.hpp>
#include <editor/commands/composite_command.hpp>

#include <editor/widgets/drag_session.hpp>

namespace editor {

/** @brief Drag payload type for a Hierarchy row, also accepted by Node-typed script fields in the Inspector. */
inline constexpr auto node_drag_drop_payload_type = "HIERARCHY_NODE";

/** @brief An asset file's kind, inferred from its extension. */
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
  scene, // no cook step, but manifest-registered like prefab
  script, // reference-only, compiled by scripting::script_compiler
}; // enum class asset_kind

/** @brief Nothing is selected. */
struct empty_selection { };

/**
 * @brief One or more selected scene nodes, in selection order; ids.back() is the primary node.
 *
 * range_anchor is the last explicitly clicked row that Shift+click ranges from; it may no longer be selected.
 */
struct node_selection {
  std::vector<sbx::math::uuid> ids{};
  sbx::math::uuid range_anchor{sbx::math::uuid::nil()};
}; // struct node_selection

/** @brief An asset file is selected, from the Asset Browser. */
struct asset_selection {
  sbx::math::uuid id{sbx::math::uuid::nil()};
  std::filesystem::path path{};               // relative to the assets directory
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

struct gizmo_node_drag_before {
  sbx::math::uuid node_id{sbx::math::uuid::nil()};
  sbx::scenes::local_transform transform{};
}; // struct gizmo_node_drag_before

// Every selected node's pre-drag world and local transform, keyed by uuid.
struct gizmo_group_drag_state {
  bool active{false};
  sbx::math::matrix4x4 pivot_before{sbx::math::matrix4x4::identity};
  std::unordered_map<sbx::math::uuid, sbx::math::matrix4x4> world_before{};
  std::unordered_map<sbx::math::uuid, sbx::scenes::local_transform> local_before{};
}; // struct gizmo_group_drag_state

/** @brief State shared across editor panels: selection, gizmo settings, requests between panels and the undo history. Panel-private state lives on the panel. */
struct editor_state {

  selection current_selection{empty_selection{}};

  // Ctrl+C'd subtrees (serialize_subtree snapshots with original ids).
  std::vector<YAML::Node> node_clipboard{};

  gizmo_operation current_gizmo_operation{gizmo_operation::translate};
  gizmo_mode current_gizmo_mode{gizmo_mode::world};

  // Cross-frame viewport gizmo drags, each bracketed into one undo entry (see viewport_transform_gizmo.cpp).
  drag_session<gizmo_node_drag_before> gizmo_node_drag{};
  gizmo_group_drag_state gizmo_group_drag{};

  /**
   * @brief The primary selected node, re-resolved by uuid.
   *
   * @param scene The scene to resolve against.
   *
   * @return The node, or an invalid node if no node is selected or it no longer exists.
   */
  [[nodiscard]] auto selected_node(sbx::scenes::scene& scene) const -> sbx::scenes::node;

  /**
   * @brief Whether @p node is anywhere in the selection.
   *
   * @param node The node to test.
   *
   * @return True if selected.
   */
  [[nodiscard]] auto is_node_selected(const sbx::scenes::node& node) const noexcept -> bool;

  /**
   * @brief Selects only @p node and makes it the range anchor.
   *
   * @param node The node to select.
   */
  auto select_node(const sbx::scenes::node& node) -> void;

  /**
   * @brief Ctrl+click: toggles @p node's membership and makes it the range anchor.
   *
   * @param node The node to toggle.
   */
  auto toggle_node_selection(const sbx::scenes::node& node) -> void;

  /**
   * @brief Shift+click in the viewport: adds @p node if absent, never removes. Makes it the range anchor.
   *
   * @param node The node to add.
   */
  auto add_node_to_selection(const sbx::scenes::node& node) -> void;

  /**
   * @brief Replaces the selection with @p ids in order, keeping the range anchor; used by the Hierarchy's range select.
   *
   * @param ids The new selection; empty clears it.
   */
  auto set_node_selection(std::vector<sbx::math::uuid> ids) -> void;

  /**
   * @brief The selected node ids in selection order.
   *
   * @return The ids, empty when no node is selected.
   */
  [[nodiscard]] auto selected_node_ids() const -> const std::vector<sbx::math::uuid>&;

  /**
   * @brief The Hierarchy's Shift+click range anchor.
   *
   * @return The anchor, or nil without a node selection.
   */
  [[nodiscard]] auto node_selection_anchor() const -> sbx::math::uuid;

  /**
   * @brief The number of selected nodes.
   *
   * @return The count, 0 when no node is selected.
   */
  [[nodiscard]] auto selected_node_count() const -> std::size_t;

  auto select_asset(sbx::math::uuid id, std::filesystem::path path, asset_kind kind) -> void;

  auto clear_selection() -> void;

  /**
   * @brief Asks the Asset Browser once to reveal the folder containing @p path, without changing the selection.
   *
   * @param path The project-relative path to reveal.
   */
  auto request_reveal_in_browser(std::filesystem::path path) -> void {
    reveal_in_browser_request = std::move(path);
  }

  std::optional<std::filesystem::path> reveal_in_browser_request{};

  /** @brief One-shot request to open the animation graph editor, consumed by animation_graph_panel. */
  struct animation_graph_edit_request {
    sbx::math::uuid id{sbx::math::uuid::nil()};
    std::filesystem::path path{}; // project-relative
    sbx::math::uuid preview_mesh_id{sbx::math::uuid::nil()}; // nil when opened without a mesh in context
  }; // struct animation_graph_edit_request

  auto request_open_animation_graph_editor(sbx::math::uuid id, std::filesystem::path path, sbx::math::uuid preview_mesh_id = sbx::math::uuid::nil()) -> void {
    open_animation_graph_request = animation_graph_edit_request{id, std::move(path), preview_mesh_id};
  }

  std::optional<animation_graph_edit_request> open_animation_graph_request{};

  /** @brief One-shot request to open the shader graph editor, consumed by shader_graph_panel. */
  struct shader_graph_edit_request {
    sbx::math::uuid id{sbx::math::uuid::nil()};
    std::filesystem::path path{}; // project-relative
  }; // struct shader_graph_edit_request

  auto request_open_shader_graph_editor(sbx::math::uuid id, std::filesystem::path path) -> void {
    open_shader_graph_request = shader_graph_edit_request{id, std::move(path)};
  }

  std::optional<shader_graph_edit_request> open_shader_graph_request{};

  /** @brief One-shot request to open a scene, consumed by editor_ui_layer's guarded open_scene(). */
  struct scene_edit_request {
    std::filesystem::path path{}; // project-relative
  }; // struct scene_edit_request

  auto request_open_scene(std::filesystem::path path) -> void {
    open_scene_request = scene_edit_request{std::move(path)};
  }

  std::optional<scene_edit_request> open_scene_request{};

  /** @brief One-shot request to open Project Settings on its Layers tab. */
  auto request_open_layer_settings() -> void {
    open_layer_settings_request = true;
  }

  bool open_layer_settings_request{false};

  // The scene undo/redo history; prefer the pass-throughs below.
  command_stack commands{};

  // The other selected nodes while the Inspector draws a multi-selection, so its edits reach every selected node (command::broadcast).
  std::vector<sbx::math::uuid> broadcast_targets{};

  auto push_command(sbx::scenes::scene& target, std::unique_ptr<command> cmd) -> void {
    if (!broadcast_targets.empty()) {
      if (auto others = cmd->broadcast(target, broadcast_targets); !others.empty()) {
        auto label = cmd->label();

        others.insert(others.begin(), std::move(cmd));
        cmd = std::make_unique<composite_command>(std::move(others), std::move(label));
      }
    }

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

  /** @brief Drops the undo history and any in-flight gizmo drag -- both refer to the scene being replaced. */
  auto clear_command_stack() -> void {
    commands.clear();
    gizmo_node_drag = {};
    gizmo_group_drag = {};
  }

}; // struct editor_state

} // namespace editor

#endif // EDITOR_EDITOR_STATE_HPP_
