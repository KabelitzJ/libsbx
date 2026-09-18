// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_SHADER_GRAPH_PANEL_HPP_
#define EDITOR_PANELS_SHADER_GRAPH_PANEL_HPP_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <imgui_node_editor.h>

#include <libsbx/assets/shader_graph.hpp>

#include <editor/panels/editor_panel.hpp>
#include <editor/panels/shader_graph_preview_renderer.hpp>
#include <editor/panels/shader_graph_node_preview_manager.hpp>

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
 * create_info (_edit), with an explicit Save button for disk persistence -- no undo/redo (asset
 * edits never go through editor_state's command_stack). Unlike those two, a change here does NOT
 * recompile live: every node/edge edit used to route through assets_module::update_shader_graph,
 * which bumps the graph's generation and re-cooks -- cheap codegen, but the very next frame's
 * render-pass pipeline lookup then guaranteed-misses shader_cache and blocks the render thread on a
 * full synchronous Slang compile, freezing the editor for however long that takes. Editing now only
 * runs cheap, in-memory validation (_apply_live, reusing generate_shader_graph_source itself --
 * discarding its generated source, keeping only success/error) and syncs _edit into the resident
 * graph (assets_module::update_shader_graph_data, no recompile) so a node drag
 * (update_shader_graph_node_position) can still find a freshly-added node; the actual re-cook only
 * happens on Save (asset_residency::save_shader_graph) -- the *scene* viewport, that is: the
 * panel's own master preview (_preview, shader_graph_preview_renderer) does recompile live, the
 * way Unity's own node previews do, but asynchronously (shader_graph_preview_renderer owns its own
 * async_shader_compiler) so it never blocks the render thread the way the scene path used to.
 * _apply_live's `structural` parameter is what tells it when that's actually needed: a structural
 * edit (add/delete a node or edge, rewire a swizzle pattern, toggle Exposed) enqueues a preview
 * recompile; a value-only edit (dragging a Constant's own value) doesn't -- the preview shader
 * already reads exposed values through its own small per-frame-refreshed buffer (mirroring how a
 * real material's generic_params work), so a value change just needs a redraw, not a recompile.
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

  // Called after every edit (and once from _open): syncs _edit into the resident graph (no
  // recompile -- see the class doc comment) and revalidates it, updating _validation_error for the
  // toolbar. Does not cook/recompile the real asset; see _draw_toolbar's Save button for that. @p
  // structural additionally enqueues a master-preview recompile (see the class doc comment) --
  // pass false from a value-only editor (a Constant's own DragFloat/color field/vector3 control).
  auto _apply_live(bool structural = true) -> void;

  auto _draw_toolbar() -> void;
  auto _draw_canvas() -> void;
  auto _draw_add_node_menu(sbx::math::vector2 spawn_position) -> void;
  auto _draw_master_preview() -> void;
  auto _draw_selection_inspector(editor_state& state) -> void;

  // A node's inline "pick one of a few string modes" trigger button (Scene Depth's Raw/Eye/
  // Linear01, Screen Position's Default/Raw, ...) -- see _mode_popup_node_id's own doc comment for
  // why this only records the request rather than opening the popup itself. `x` is this node's own
  // content_start_x (_draw_canvas's own local); `popup_requested` is _draw_canvas's own local, set
  // when this returns having recorded a click.
  auto _draw_inline_mode_trigger(const sbx::assets::shader_graph_node& node, const std::string& current_mode, const std::vector<std::pair<std::string, std::string>>& options, float x, float width, bool& popup_requested) -> void;

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

  shader_graph_node_preview_manager _node_previews{};

  // Set by _apply_live from generate_shader_graph_source's own error message -- empty means _edit
  // would cook cleanly right now. Shown in the toolbar in place of the old, narrower
  // "no Fragment output" check it replaced (a strict superset: missing-Fragment-output is one of
  // the errors generate_shader_graph_source itself already reports).
  std::string _validation_error{};

  shader_graph_preview_renderer _preview{};

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

  // Which node's inline color swatch / string-mode picker (Scene Depth's Raw/Eye/Linear01, Screen
  // Position's Default/Raw, ...) a popup-based picker currently targets -- same "captured once, kept
  // across however many frames the popup stays open" reasoning as _context_node_id above, and for
  // the same underlying cause: imgui-node-editor applies its own pan/zoom coordinate transform
  // inside BeginNode/EndNode, so a popup (Combo's dropdown, a color picker) opened directly from
  // inside a node renders at the wrong screen position/doesn't receive input -- the library's own
  // "widgets" example (relevant upstream bug: thedmd/imgui-node-editor#48) works around this by only
  // recording that a popup was requested INSIDE the node, then actually calling OpenPopup/BeginPopup
  // OUTSIDE BeginNode/EndNode entirely, wrapped in Suspend()/Resume() (see _draw_canvas's own
  // Suspend/Resume block, shared with the node/background context menus).
  std::optional<std::uint32_t> _color_popup_node_id{};

  // One shared popup serves every string-mode node type (only one such popup can be open at a time
  // anyway) -- _mode_popup_options is copied in as (label, value) pairs when the popup is requested,
  // since the option list differs per node type.
  std::optional<std::uint32_t> _mode_popup_node_id{};
  std::vector<std::pair<std::string, std::string>> _mode_popup_options{};

  ax::NodeEditor::EditorContext* _context{nullptr};

}; // class shader_graph_panel

} // namespace editor

#endif // EDITOR_PANELS_SHADER_GRAPH_PANEL_HPP_
