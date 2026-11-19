// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Jonas Kabelitz
#ifndef EDITOR_PANELS_ANIMATION_GRAPH_PANEL_HPP_
#define EDITOR_PANELS_ANIMATION_GRAPH_PANEL_HPP_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <unordered_set>
#include <variant>

#include <imgui_node_editor.h>

#include <libsbx/assets/animation_graph.hpp>
#include <libsbx/assets/mesh.hpp>

#include <editor/panels/editor_panel.hpp>

namespace editor {

/**
 * @brief The visual animation state graph editor on imgui-node-editor, opened per `.animation_graph` asset via editor_state::request_open_animation_graph_editor.
 *
 * Edits a staged create_info applied live through update_animation_graph, with an explicit Save and no undo.
 */
class animation_graph_panel final : public editor_panel {

public:

  animation_graph_panel();
  ~animation_graph_panel() override;

  animation_graph_panel(const animation_graph_panel&) = delete;
  auto operator=(const animation_graph_panel&) -> animation_graph_panel& = delete;

  auto draw(editor_state& state) -> void override;

private:

  // monostate: nothing; std::uint32_t: a state id; std::size_t: an index into _edit.transitions.
  using selection = std::variant<std::monostate, std::uint32_t, std::size_t>;

  auto _open(sbx::assets::animation_graph_handle graph, std::filesystem::path path, sbx::assets::mesh_handle preview_mesh = {}) -> void;
  auto _apply_live() -> void;

  auto _draw_toolbar() -> void;
  auto _draw_parameters() -> void;
  auto _draw_canvas() -> void;
  auto _draw_selection_inspector() -> void;

  /** @brief The layer tabs, and the selected layer's name, weight, blend mode, mask and order. */
  auto _draw_layers() -> void;

  /**
   * @brief Edits a layer's joint mask: a joint tree of the preview mesh's skeleton (ticking a joint ticks its descendants), or a plain name list without one.
   *
   * @param layer The layer whose mask to edit.
   *
   * @return Whether the mask changed.
   */
  auto _draw_mask_editor(sbx::assets::animation_layer& layer) -> bool;

  /** @brief The layer the canvas and inspector edit. */
  [[nodiscard]] auto _layer() -> sbx::assets::animation_layer& {
    return _edit.layers[std::min(_layer_index, _edit.layers.size() - 1u)];
  }

  [[nodiscard]] auto _layer() const -> const sbx::assets::animation_layer& {
    return _edit.layers[std::min(_layer_index, _edit.layers.size() - 1u)];
  }

  /**
   * @brief Edits a state's motion: a clip picker, or a blend's type, parameters and children, recursively.
   *
   * @param motion The motion to edit.
   *
   * @return Whether anything changed.
   */
  auto _draw_motion(sbx::assets::animation_motion& motion) -> bool;

  /**
   * @brief A combo over the graph's float and int parameters.
   *
   * @param label The combo's label.
   * @param name The selected parameter's name, empty for none.
   *
   * @return Whether the selection changed.
   */
  auto _draw_float_parameter_combo(const char* label, std::string& name) -> bool;

  /**
   * @brief Plots a 2D blend's children and the point the parameters' default values select, each child sized by its weight there.
   *
   * @param motion A 2D blend motion.
   * @param weights The children's weights at the default values.
   */
  auto _draw_blend_preview(const sbx::assets::animation_motion& motion, std::span<const std::float_t> weights) -> void;

  [[nodiscard]] auto _state_name(std::uint32_t state_id) const -> std::string;

  bool _is_open{false};
  sbx::assets::animation_graph_handle _graph{};
  std::filesystem::path _path{};
  sbx::assets::animation_graph::create_info _edit{};
  selection _selection{};

  // Index into _edit.layers of the layer being edited; _open keeps at least one layer.
  std::size_t _layer_index{0u};

  // Set when a button changed _layer_index, so the layer tabs follow.
  bool _select_layer_tab{false};

  // The mesh whose clips Clip Name lists; editor-only, seeded from the opening context and overridable.
  sbx::assets::mesh_handle _preview_mesh{};

  // States whose editor position was seeded since the last _open(); the node editor remembers positions after that.
  std::unordered_set<std::uint32_t> _seeded_positions{};

  // The Any State pseudo-node isn't in the asset, so its position is editor-local.
  bool _any_state_seeded{false};

  ax::NodeEditor::EditorContext* _context{nullptr};

}; // class animation_graph_panel

} // namespace editor

#endif // EDITOR_PANELS_ANIMATION_GRAPH_PANEL_HPP_
