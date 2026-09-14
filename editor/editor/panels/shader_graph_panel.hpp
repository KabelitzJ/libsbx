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
 * @brief The visual shader-graph editor: nodes as draggable boxes with typed input pins (0-3,
 * per shader_node_input_count) and at most one output pin, edges as links between them, built on
 * imgui-node-editor -- structurally the same tool animation_graph_panel already uses, just for a
 * dataflow graph instead of a state machine (multiple typed input pins per node, rather than one
 * input/one output). On-demand rather than a fixed dockspace fixture, same reasoning as
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

  bool _is_open{false};
  sbx::assets::shader_graph_handle _graph{};
  std::filesystem::path _path{};
  sbx::assets::shader_graph::create_info _edit{};
  selection _selection{};

  // Which node ids have already had ax::NodeEditor::SetNodePosition seeded from
  // shader_graph_node::editor_position since the last _open() -- same reasoning as
  // animation_graph_panel's own _seeded_positions.
  std::unordered_set<std::uint32_t> _seeded_positions{};

  ax::NodeEditor::EditorContext* _context{nullptr};

}; // class shader_graph_panel

} // namespace editor

#endif // EDITOR_PANELS_SHADER_GRAPH_PANEL_HPP_
