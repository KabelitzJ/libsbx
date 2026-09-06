// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_ANIMATION_GRAPH_PANEL_HPP_
#define EDITOR_PANELS_ANIMATION_GRAPH_PANEL_HPP_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <variant>

#include <imgui_node_editor.h>

#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/mesh.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief The visual state-graph editor: states as draggable nodes, transitions as links between
 * them, built on imgui-node-editor. On-demand rather than a fixed dockspace fixture (unlike
 * hierarchy/inspector/asset_browser/logger) — draws nothing until opened for a specific
 * .animation_graph asset via editor_state::request_open_animation_graph_editor, since a node
 * canvas needs real screen space the Inspector's narrow dock doesn't have.
 *
 * Edits the same way _draw_particle_effect_properties edits a particle_effect: a staged
 * create_info (_edit) applied live via assets_module::update_animation_graph on every change, with
 * an explicit Save button for disk persistence -- no undo/redo (asset edits never go through
 * editor_state's command_stack, only scene-node component edits do).
 */
class animation_graph_panel final : public editor_panel {

public:

  animation_graph_panel();
  ~animation_graph_panel() override;

  animation_graph_panel(const animation_graph_panel&) = delete;
  auto operator=(const animation_graph_panel&) -> animation_graph_panel& = delete;

  auto draw(editor_state& state) -> void override;

private:

  // monostate: nothing selected. std::uint32_t: an animation_state::id. std::size_t: an index into
  // _edit.transitions.
  using selection = std::variant<std::monostate, std::uint32_t, std::size_t>;

  auto _open(sbx::assets::animation_graph_handle graph, std::filesystem::path path, sbx::assets::mesh_handle preview_mesh = {}) -> void;
  auto _apply_live() -> void;

  auto _draw_toolbar() -> void;
  auto _draw_parameters() -> void;
  auto _draw_canvas() -> void;
  auto _draw_selection_inspector() -> void;

  [[nodiscard]] auto _state_name(std::uint32_t state_id) const -> std::string;

  bool _is_open{false};
  sbx::assets::animation_graph_handle _graph{};
  std::filesystem::path _path{};
  sbx::assets::animation_graph::create_info _edit{};
  selection _selection{};

  // Which mesh's animation_clips() to list a state's Clip Name from -- editor-only, never
  // persisted into the asset (the same graph can in principle drive different meshes, as long as
  // their clip names agree). Seeded from whatever mesh was in scope when the editor was opened
  // (an Animator's own mesh_renderer), overridable below the toolbar's Save button.
  sbx::assets::mesh_handle _preview_mesh{};

  // Which state ids have already had ax::NodeEditor::SetNodePosition seeded from animation_state::editor_position
  // since the last _open() -- imgui-node-editor remembers a node's position itself once set, so this
  // is only needed once per state per editor context, not every frame.
  std::unordered_set<std::uint32_t> _seeded_positions{};

  // "Any State" pseudo-node's canvas position -- not part of the asset (from_state == nullopt has
  // no animation_state to carry a position), so it's editor-context-local only.
  bool _any_state_seeded{false};

  ax::NodeEditor::EditorContext* _context{nullptr};

}; // class animation_graph_panel

} // namespace editor

#endif // EDITOR_PANELS_ANIMATION_GRAPH_PANEL_HPP_
