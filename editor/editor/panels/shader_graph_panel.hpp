// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_SHADER_GRAPH_PANEL_HPP_
#define EDITOR_PANELS_SHADER_GRAPH_PANEL_HPP_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <variant>

#include <imgui_node_editor.h>

#include <libsbx/assets/shader_graph.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief The visual shader-graph editor: nodes as draggable boxes with typed input pins (0-7, per
 * shader_node_input_count) and one or more typed output pins (0-4, per shader_node_output_count --
 * every node but Split, which has X/Y/Z/W, has exactly one), edges as links between them, built on
 * imgui-node-editor -- structurally the same tool animation_graph_panel already uses, just for a
 * dataflow graph instead of a state machine (multiple typed pins per node, rather than one input/
 * one output). Every pin has a fixed or wiring-resolved shader_value_type (shader_graph.hpp's
 * shader_graph_type_resolver); a connection is only legal when the source's type exactly matches
 * the target's -- no implicit narrowing/widening, Split/Combine are the explicit tools for that. On-
 * demand rather than a fixed dockspace fixture, same reasoning as
 * animation_graph_panel's own doc comment: opened for a specific `.shadergraph` asset via
 * editor_state::request_open_shader_graph_editor, not part of the default dock layout.
 *
 * Edits the same way animation_graph_panel/_draw_particle_effect_properties do: a staged
 * create_info (_edit) applied live via assets_module::update_shader_graph on every change (which
 * also re-cooks the generated `.slang`, see asset_residency::update_shader_graph), with an
 * explicit Save button for disk persistence -- no undo/redo (asset edits never go through
 * editor_state's command_stack).
 */
class shader_graph_panel final : public editor_panel {

public:

  shader_graph_panel();
  ~shader_graph_panel() override;

  shader_graph_panel(const shader_graph_panel&) = delete;
  auto operator=(const shader_graph_panel&) -> shader_graph_panel& = delete;

  auto draw(editor_state& state) -> void override;

private:

  // monostate: nothing selected. std::uint32_t: a shader_graph_node::id. std::size_t: an index
  // into _edit.edges.
  using selection = std::variant<std::monostate, std::uint32_t, std::size_t>;

  auto _open(sbx::assets::shader_graph_handle graph, std::filesystem::path path) -> void;
  auto _apply_live() -> void;

  auto _draw_toolbar() -> void;
  auto _draw_canvas() -> void;
  auto _draw_add_node_menu(sbx::math::vector2 spawn_position) -> void;
  auto _draw_selection_inspector() -> void;

  [[nodiscard]] auto _next_node_id() const -> std::uint32_t;

  // Pushes a new node into _edit.nodes and returns its id -- shared by _draw_add_node_menu's own
  // palette entries and the source nodes it auto-spawns alongside certain node types (Texture
  // Sample, Vertex) so a freshly added node isn't left with an implicit/no-op default; see its call
  // sites for exactly which types trigger that and why.
  auto _spawn_node(sbx::assets::shader_node_type type, sbx::math::vector2 position) -> std::uint32_t;

  // Spawns `type` at `spawn_position` via _spawn_node, wires in its auto-wired defaults if it has
  // any (see _spawn_node's own doc comment), and selects it -- the one path both the Add Node
  // menu's categorized browsing and its text search commit through, so they can never drift.
  auto _add_node(sbx::assets::shader_node_type type, sbx::math::vector2 spawn_position) -> void;

  bool _is_open{false};
  sbx::assets::shader_graph_handle _graph{};
  std::filesystem::path _path{};
  sbx::assets::shader_graph::create_info _edit{};
  selection _selection{};

  // Which node ids have already had ax::NodeEditor::SetNodePosition seeded from
  // shader_graph_node::editor_position since the last _open() -- same reasoning as
  // animation_graph_panel's own _seeded_positions.
  std::unordered_set<std::uint32_t> _seeded_positions{};

  // Add Node popup's search box -- cleared (and keyboard focus requested) each time the popup
  // freshly opens, see the ShowBackgroundContextMenu call site in _draw_canvas.
  std::string _add_node_search{};
  bool _add_node_search_focus_pending{false};

  // Where a node picked from the Add Node popup spawns -- captured once, at the exact moment the
  // popup opens (the canvas position under the right-click that triggered it), not re-read every
  // frame the popup stays open while the user browses or searches.
  sbx::math::vector2 _add_node_spawn_position{};

  // Which node the right-click node context menu ("Delete Node") applies to -- captured once, the
  // frame ShowNodeContextMenu actually detects the right-click, and kept for however many
  // subsequent frames the popup stays open (ShowNodeContextMenu itself only reports a node on that
  // one triggering frame). See the ShowNodeContextMenu call site in _draw_canvas.
  ax::NodeEditor::NodeId _context_node_id{};

  ax::NodeEditor::EditorContext* _context{nullptr};

}; // class shader_graph_panel

} // namespace editor

#endif // EDITOR_PANELS_SHADER_GRAPH_PANEL_HPP_
