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
 * @brief The visual shader graph editor on imgui-node-editor, opened per `.shadergraph` asset via editor_state::request_open_shader_graph_editor.
 *
 * Pins have fixed or wiring-resolved types, and connections must match exactly; Split and Combine convert explicitly.
 * Edits a staged create_info with an explicit Save and no undo. Edits only validate and sync the resident graph without re-cooking, since a re-cook blocks the render thread on a Slang compile; Save re-cooks.
 * The panel's own previews recompile asynchronously on structural edits; value edits reach them through the preview material buffer.
 */
class shader_graph_panel final : public editor_panel {

public:

  shader_graph_panel();
  ~shader_graph_panel() override;

  shader_graph_panel(const shader_graph_panel&) = delete;
  auto operator=(const shader_graph_panel&) -> shader_graph_panel& = delete;

  auto draw(editor_state& state) -> void override;

private:

  struct property_selection {
    std::uint32_t id{};
  }; // struct property_selection

  // monostate: nothing; std::uint32_t: a node id; std::size_t: an index into _edit.edges; property_selection: a blackboard property.
  using selection = std::variant<std::monostate, std::uint32_t, std::size_t, property_selection>;

  auto _open(sbx::assets::shader_graph_handle graph, std::filesystem::path path) -> void;

  // After every edit: syncs _edit into the resident graph without re-cooking and revalidates it. @p structural also recompiles the previews; pass false for value-only edits.
  auto _apply_live(bool structural = true) -> void;

  auto _draw_toolbar() -> void;
  auto _draw_canvas() -> void;
  auto _draw_add_node_menu(sbx::math::vector2 spawn_position) -> void;
  auto _draw_master_preview() -> void;
  auto _draw_selection_inspector(editor_state& state) -> void;

  auto _draw_blackboard() -> void;
  auto _draw_property_inspector(editor_state& state, sbx::assets::shader_graph_property& property) -> void;

  auto _add_property(sbx::assets::shader_graph_parameter_type type) -> std::uint32_t;
  auto _rename_property(std::uint32_t id, const std::string& name) -> void;

  // Removes the property, its nodes and their edges.
  auto _delete_property(std::uint32_t id) -> void;

  // Moves property @p id to blackboard position @p index; its slot doesn't change.
  auto _move_property(std::uint32_t id, std::size_t index) -> void;

  auto _spawn_property_node(std::uint32_t property_id, sbx::math::vector2 position) -> std::uint32_t;

  // Constant -> property node, or an inline Sample Texture -> a texture property wired into its Texture input.
  auto _convert_to_property(std::uint32_t node_id) -> void;

  // Property node -> constant, or a texture property node -> its texture inlined into the Sample Textures it fed.
  auto _convert_to_inline(std::uint32_t node_id) -> void;

  auto _select_property(std::uint32_t id) -> void;

  // A node's inline mode button; only records the click, since popups must open outside the node (see _mode_popup_node_id).
  auto _draw_inline_mode_trigger(const sbx::assets::shader_graph_node& node, const std::string& current_mode, const std::vector<std::pair<std::string, std::string>>& options, float x, float width, bool& popup_requested) -> void;

  [[nodiscard]] auto _next_node_id() const -> std::uint32_t;

  // Adds a node to _edit.nodes and returns its id.
  auto _spawn_node(sbx::assets::shader_node_type type, sbx::math::vector2 position) -> std::uint32_t;

  // Spawns a node, wires its auto-spawned defaults and selects it; shared by the menu's browsing and search.
  auto _add_node(sbx::assets::shader_node_type type, sbx::math::vector2 spawn_position) -> void;

  bool _is_open{false};
  sbx::assets::shader_graph_handle _graph{};
  std::filesystem::path _path{};
  sbx::assets::shader_graph::create_info _edit{};
  selection _selection{};

  shader_graph_node_preview_manager _node_previews{};

  // generate_shader_graph_source's error for _edit, shown in the toolbar; empty when it would cook.
  std::string _validation_error{};

  shader_graph_preview_renderer _preview{};

  // Nodes whose editor position was seeded since the last _open().
  std::unordered_set<std::uint32_t> _seeded_positions{};

  // Cleared, with focus requested, each time the Add Node popup opens.
  std::string _add_node_search{};
  bool _add_node_search_focus_pending{false};

  // Captured when the Add Node popup opens, so nodes spawn under the right-click.
  sbx::math::vector2 _add_node_spawn_position{};

  // The context menu's node, kept while the popup is open since ShowNodeContextMenu only reports it on the right-click frame.
  ax::NodeEditor::NodeId _context_node_id{};

  // The node a color or mode popup targets. Popups opened inside a node render at the wrong position under the editor's pan/zoom (thedmd/imgui-node-editor#48), so they're opened outside BeginNode/EndNode within Suspend()/Resume().
  std::optional<std::uint32_t> _color_popup_node_id{};

  // One popup serves every mode node type; the per-type options are copied in when it's requested.
  std::optional<std::uint32_t> _mode_popup_node_id{};
  std::vector<std::pair<std::string, std::string>> _mode_popup_options{};

  // Inline rename of a blackboard row, started by double-click or the row's context menu.
  std::optional<std::uint32_t> _renaming_property_id{};
  std::string _rename_buffer{};
  bool _rename_focus_pending{false};

  // Set when a blackboard row is selected; _draw_canvas() clears the node editor's selection so it doesn't take over again.
  bool _clear_canvas_selection_pending{false};

  ax::NodeEditor::EditorContext* _context{nullptr};

}; // class shader_graph_panel

} // namespace editor

#endif // EDITOR_PANELS_SHADER_GRAPH_PANEL_HPP_
